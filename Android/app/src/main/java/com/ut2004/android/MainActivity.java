package com.ut2004.android;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.provider.Settings;
import android.util.Log;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.widget.Switch;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;

/**
 * Launcher (Stage 2.11). Gates All-Files-Access (the ~11 GB game tree is read in place from external
 * storage), then -- instead of auto-launching the game -- shows a launcher screen that lets the
 * player pick which storage the game runs from and manage a fast local copy.
 *
 * The ~11 GB tree normally lives on external storage (/sdcard/UT2004 or a physical SD card), which
 * goes through Android's FUSE/sdcardfs layer and is punishingly slow (the Retroid takes ~3 min to
 * reach the menu). The only fast, non-FUSE storage is app-private /data ({@code getFilesDir()}), so
 * this screen can copy the game there and boot from it. The storage choice is conveyed to the
 * separate ":game" process via an Intent extra ({@link GameActivity#EXTRA_STORAGE_MODE}); default-mode
 * SharedPreferences would not be multi-process safe.
 *
 * All widgets are built programmatically (the app has no androidx/Material dep and no XML layouts),
 * the same pattern as GameActivity's touch overlay.
 */
public class MainActivity extends Activity {
    private static final String TAG = "UT2004";
    private static final int COUNTDOWN_SECS = 10;

    private boolean launcherShown = false;
    private boolean opRunning = false;
    private boolean gameLaunched = false;

    private Switch storageSwitch;
    private Button startButton, copyButton, clearButton, copySettingsButton;
    private TextView statusText, progressText;
    private ProgressBar progressBar;

    private final Handler ui = new Handler(Looper.getMainLooper());
    private boolean countdownActive = false;
    private int countdownRemaining = 0;
    private final Runnable countdownTick = new Runnable() {
        @Override public void run() {
            if (!countdownActive) return;
            countdownRemaining--;
            if (countdownRemaining <= 0) {
                countdownActive = false;
                launchGame();
            } else {
                startButton.setText("Start game (" + countdownRemaining + ")");
                ui.postDelayed(this, 1000L);
            }
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (!hasAllFilesAccess()) {
            requestAllFilesAccess();
            return;
        }
        gameLaunched = false; // returning from the game (or first show) -- re-arm Start
        if (!launcherShown) {
            showLauncher();
        } else {
            // Returning from the game (Back) or the settings screen -- refresh, but don't auto-relaunch.
            cancelCountdown(true);
            refreshStatusAsync();
        }
    }

    // ---- storage-permission gate (unchanged from 2.10 arm-perf) ---------------------------------

    private boolean hasAllFilesAccess() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            return Environment.isExternalStorageManager();
        }
        // Android <= 10 (API <= 29, e.g. the Retroid on Android 9): legacy storage model -- the
        // engine reads the game tree (incl. a physical SD card) via runtime READ_EXTERNAL_STORAGE.
        return checkSelfPermission(android.Manifest.permission.READ_EXTERNAL_STORAGE)
                == android.content.pm.PackageManager.PERMISSION_GRANTED;
    }

    private void requestAllFilesAccess() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            // Legacy runtime storage permission (Android <= 10). onResume re-checks after the dialog.
            requestPermissions(new String[] {
                    android.Manifest.permission.READ_EXTERNAL_STORAGE,
                    android.Manifest.permission.WRITE_EXTERNAL_STORAGE }, 1);
            return;
        }
        Toast.makeText(this, "Grant All files access to UT2004, then return to this screen.",
                Toast.LENGTH_LONG).show();
        try {
            startActivity(new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                    Uri.parse("package:" + getPackageName())));
        } catch (Exception e) {
            Log.e(TAG, "per-app All-files-access settings unavailable; opening the global list", e);
            startActivity(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION));
        }
    }

    // ---- launcher UI ----------------------------------------------------------------------------

    private int dp(float v) {
        return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v,
                getResources().getDisplayMetrics());
    }

    private void showLauncher() {
        launcherShown = true;

        ScrollView scroll = new ScrollView(this);
        scroll.setBackgroundColor(0xFF0E1013);
        scroll.setFillViewport(true);

        LinearLayout col = new LinearLayout(this);
        col.setOrientation(LinearLayout.VERTICAL);
        col.setPadding(dp(24), dp(28), dp(24), dp(28));
        col.setLayoutParams(new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        scroll.addView(col);

        TextView title = new TextView(this);
        title.setText("UNREAL TOURNAMENT 2004");
        title.setTextColor(0xFFE8B34A);
        title.setTextSize(TypedValue.COMPLEX_UNIT_SP, 22f);
        title.setGravity(Gravity.CENTER);
        title.setPadding(0, 0, 0, dp(20));
        col.addView(title);

        startButton = new Button(this);
        startButton.setText("Start game");
        startButton.setTextSize(TypedValue.COMPLEX_UNIT_SP, 20f);
        startButton.setAllCaps(false);
        col.addView(startButton, wide(dp(72)));
        startButton.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { launchGame(); }
        });

        // Storage picker: unchecked (left) = Internal, checked (right) = External.
        TextView pick = new TextView(this);
        pick.setText("Run game from:");
        pick.setTextColor(0xFFB8BCC2);
        pick.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14f);
        pick.setPadding(0, dp(22), 0, dp(4));
        col.addView(pick);

        storageSwitch = new Switch(this);
        storageSwitch.setTextColor(0xFFECEFF3);
        storageSwitch.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16f);
        storageSwitch.setPadding(0, dp(2), 0, dp(2));
        storageSwitch.setOnCheckedChangeListener(new android.widget.CompoundButton.OnCheckedChangeListener() {
            @Override public void onCheckedChanged(android.widget.CompoundButton btn, boolean checked) {
                applyStorageSwitchText();
            }
        });
        applyStorageSwitchText();
        col.addView(storageSwitch);

        statusText = new TextView(this);
        statusText.setTextColor(0xFF9AA0A8);
        statusText.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13f);
        statusText.setPadding(0, dp(16), 0, dp(16));
        statusText.setText("Checking storage…");
        col.addView(statusText);

        copyButton = mkButton(col, "Copy game to internal memory");
        copyButton.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { doCopyToInternal(); }
        });

        clearButton = mkButton(col, "Clear internal memory");
        clearButton.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { doClearInternal(); }
        });

        copySettingsButton = new Button(this);
        copySettingsButton.setText("Copy settings/saves to external memory");
        copySettingsButton.setAllCaps(false);
        copySettingsButton.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12f);
        col.addView(copySettingsButton, wide(dp(44)));
        copySettingsButton.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { doCopySettingsOut(); }
        });

        progressText = new TextView(this);
        progressText.setTextColor(0xFF9AA0A8);
        progressText.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12f);
        progressText.setPadding(0, dp(18), 0, dp(4));
        progressText.setVisibility(View.GONE);
        col.addView(progressText);

        progressBar = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        progressBar.setMax(100);
        progressBar.setVisibility(View.GONE);
        col.addView(progressBar, wide(dp(12)));

        // Where-to-put-the-game hint (always shown). getExternalStorageDirectory() resolves to the
        // device's shared-storage root (e.g. /storage/emulated/0), so we can show a concrete path.
        TextView hint = new TextView(this);
        hint.setTextColor(0xFF6E747C);
        hint.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11f);
        hint.setPadding(0, dp(26), 0, 0);
        File extHint = new File(Environment.getExternalStorageDirectory(), "UT2004");
        hint.setText("Game data goes in a folder named UT2004 in your device's shared storage "
                + "(the internal “SD card” area), so it lands at:\n" + extHint
                + "\nPut the game's System, Maps, Textures… folders inside it. On devices with a "
                + "physical SD card, a UT2004 folder at the card's root is also detected.");
        col.addView(hint);

        setContentView(scroll);

        // Default the picker: Internal if a local copy exists, else External.
        storageSwitch.setChecked(!GameActivity.hasGameTree(GameActivity.internalRoot(this)));
        applyStorageSwitchText();

        refreshStatusAsync();
        startCountdown();
    }

    private Button mkButton(LinearLayout col, String text) {
        Button b = new Button(this);
        b.setText(text);
        b.setAllCaps(false);
        b.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15f);
        col.addView(b, wide(dp(52)));
        return b;
    }

    private LinearLayout.LayoutParams wide(int height) {
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, height);
        lp.topMargin = dp(6);
        return lp;
    }

    private void applyStorageSwitchText() {
        boolean external = storageSwitch.isChecked();
        storageSwitch.setText(external ? "External (SD / slow)" : "Internal (/data, fast)");
    }

    // ---- start + countdown ----------------------------------------------------------------------

    private void startCountdown() {
        countdownActive = true;
        countdownRemaining = COUNTDOWN_SECS;
        startButton.setText("Start game (" + countdownRemaining + ")");
        ui.postDelayed(countdownTick, 1000L);
    }

    private void cancelCountdown(boolean relabel) {
        if (!countdownActive) return;
        countdownActive = false;
        ui.removeCallbacks(countdownTick);
        if (relabel && startButton != null) startButton.setText("Start game");
    }

    /** First touch anywhere cancels the auto-start countdown, then passes through so the tap still acts. */
    @Override
    public boolean dispatchTouchEvent(MotionEvent ev) {
        if (countdownActive && ev.getActionMasked() == MotionEvent.ACTION_DOWN) {
            cancelCountdown(true);
        }
        return super.dispatchTouchEvent(ev);
    }

    private void launchGame() {
        if (opRunning || gameLaunched) return; // don't launch mid-copy, and never twice (countdown+tap race)
        String mode = storageSwitch.isChecked() ? GameActivity.MODE_EXTERNAL : GameActivity.MODE_INTERNAL;
        // Internal selected but no local copy: tell the user rather than silently booting External
        // (resolveDataRoot still has that fallback as a defensive net for a cold recents relaunch).
        if (GameActivity.MODE_INTERNAL.equals(mode)
                && !GameActivity.hasGameTree(GameActivity.internalRoot(this))) {
            cancelCountdown(true);
            showMessage("Internal memory is empty — tap “Copy game to internal memory” first, or switch to External.");
            return;
        }
        gameLaunched = true;
        cancelCountdown(false);
        Log.i(TAG, "launching game, storage=" + mode);
        Intent i = new Intent(this, GameActivity.class);
        i.putExtra(GameActivity.EXTRA_STORAGE_MODE, mode);
        // Forward an extra engine command line (a travel URL) if this launcher was itself started
        // with one — see GameActivity.EXTRA_ARGS. GameActivity is not exported, so this hop is the
        // only route an `adb shell am start` has into the game process.
        String extraArgs = getIntent() != null
                ? getIntent().getStringExtra(GameActivity.EXTRA_ARGS) : null;
        if (extraArgs != null && !extraArgs.trim().isEmpty()) {
            i.putExtra(GameActivity.EXTRA_ARGS, extraArgs);
            Log.i(TAG, "forwarding engine arguments: " + extraArgs);
        }
        startActivity(i);
        // Deliberately not finish() -- Back from the game returns to this launcher.
    }

    // ---- storage operations ---------------------------------------------------------------------

    private void doCopyToInternal() {
        if (opRunning) return;
        cancelCountdown(true);
        final File src = GameActivity.resolveExternalRoot(this);
        final File dst = GameActivity.internalRoot(this);
        if (!GameActivity.hasGameTree(src)) {
            showMessage("UT2004 folder not found — put the game at " + src
                    + " (it must contain a System folder), then reopen this screen.");
            return;
        }
        startOp("Copying to internal memory…");
        new Thread(new Runnable() {
            @Override public void run() {
                final String err = Storage.syncTree(src, dst, throttledListener());
                final String msg = err == null ? "Copy complete — " + dst : err;
                ui.post(new Runnable() { @Override public void run() { finishOp(msg); } });
            }
        }, "ut2004-copy").start();
    }

    private void doClearInternal() {
        if (opRunning) return;
        cancelCountdown(true);
        final File dst = GameActivity.internalRoot(this);
        if (!dst.exists()) { showMessage("Internal memory is already empty."); return; }
        new AlertDialog.Builder(this)
                .setTitle("Clear internal memory?")
                .setMessage("Delete the internal copy at\n" + dst + "\n(" + Storage.human(Storage.dirSize(dst)) + ")?")
                .setPositiveButton("Delete", new DialogInterface.OnClickListener() {
                    @Override public void onClick(DialogInterface d, int w) {
                        startOp("Clearing internal memory…");
                        new Thread(new Runnable() {
                            @Override public void run() {
                                final boolean ok = Storage.deleteTree(dst);
                                final String msg = ok ? "Internal memory cleared."
                                        : "Some files could not be deleted.";
                                ui.post(new Runnable() { @Override public void run() { finishOp(msg); } });
                            }
                        }, "ut2004-clear").start();
                    }
                })
                .setNegativeButton("Cancel", null)
                .show();
    }

    private void doCopySettingsOut() {
        if (opRunning) return;
        cancelCountdown(true);
        final File internal = GameActivity.internalRoot(this);
        final File external = GameActivity.resolveExternalRoot(this);
        if (!GameActivity.hasGameTree(internal)) {
            showMessage("No internal copy to export settings from — copy the game to internal memory first.");
            return;
        }
        startOp("Copying settings/saves to external…");
        new Thread(new Runnable() {
            @Override public void run() {
                // Known settings files + the whole Save/ dir (no clean API for "settings/saves").
                StringBuilder err = new StringBuilder();
                int files = 0;
                for (String rel : new String[] { "System/User.ini", "System/UT2004.ini" }) {
                    File s = new File(internal, rel);
                    if (s.isFile()) {
                        String e = Storage.copyFileInto(s, new File(external, rel));
                        if (e != null) err.append(rel).append(": ").append(e).append('\n');
                        else files++;
                    }
                }
                File saveSrc = new File(internal, "Save");
                boolean savedSaves = false;
                if (saveSrc.isDirectory()) {
                    String e = Storage.syncTree(saveSrc, new File(external, "Save"), throttledListener());
                    if (e != null) err.append("Save/: ").append(e).append('\n');
                    else savedSaves = true;
                }
                final String msg = err.length() > 0
                        ? "Export finished with errors:\n" + err
                        : "Exported " + files + " settings file(s)"
                            + (savedSaves ? " + Save/" : "") + " to " + external;
                ui.post(new Runnable() { @Override public void run() { finishOp(msg); } });
            }
        }, "ut2004-export").start();
    }

    private Storage.Listener throttledListener() {
        return new Storage.Listener() {
            long lastMs = 0L;
            @Override public void onProgress(long done, long total, long copied, String rel) {
                long now = SystemClock.uptimeMillis();
                if (now - lastMs < 150L && done < total) return; // throttle UI floods; always show the last
                lastMs = now;
                final int pct = total > 0 ? (int) (done * 100L / total) : 0;
                final String line = "Copied " + Storage.human(copied) + "  •  " + rel;
                ui.post(new Runnable() { @Override public void run() { updateProgress(line, pct); } });
            }
        };
    }

    // ---- op UI state ----------------------------------------------------------------------------

    private void startOp(String label) {
        opRunning = true;
        setControlsEnabled(false);
        progressText.setVisibility(View.VISIBLE);
        progressText.setText(label);
        progressBar.setVisibility(View.VISIBLE);
        progressBar.setProgress(0);
    }

    private void updateProgress(String line, int pct) {
        progressText.setText(line);
        progressBar.setProgress(pct);
    }

    private void finishOp(String message) {
        opRunning = false;
        setControlsEnabled(true);
        progressBar.setVisibility(View.GONE);
        progressText.setVisibility(View.VISIBLE);
        progressText.setText(message);
        Log.i(TAG, "op done: " + message);
        refreshStatusAsync();
    }

    private void setControlsEnabled(boolean enabled) {
        startButton.setEnabled(enabled);
        copyButton.setEnabled(enabled);
        clearButton.setEnabled(enabled);
        copySettingsButton.setEnabled(enabled);
        storageSwitch.setEnabled(enabled);
    }

    // ---- status readout -------------------------------------------------------------------------

    private void refreshStatusAsync() {
        if (statusText == null) return;
        new Thread(new Runnable() {
            @Override public void run() {
                File internal = GameActivity.internalRoot(MainActivity.this);
                File external = GameActivity.resolveExternalRoot(MainActivity.this);
                long free = Storage.freeBytes(getFilesDir());
                boolean intReady = GameActivity.hasGameTree(internal);
                long intSize = intReady ? Storage.dirSize(internal) : 0L;
                boolean extReady = GameActivity.hasGameTree(external);
                long extSize = extReady ? Storage.dirSize(external) : 0L;

                StringBuilder sb = new StringBuilder();
                sb.append("Internal (/data) free: ").append(Storage.human(free)).append('\n');
                sb.append("Internal copy: ").append(intReady
                        ? Storage.human(intSize) + "  " + internal
                        : "not present").append('\n');
                sb.append("External tree: ").append(extReady
                        ? Storage.human(extSize) + "  " + external
                        : "not found");
                final String s = sb.toString();
                ui.post(new Runnable() { @Override public void run() { statusText.setText(s); } });
            }
        }, "ut2004-status").start();
    }

    private void toast(String s) {
        Toast.makeText(this, s, Toast.LENGTH_LONG).show();
    }

    /** Surface a message both persistently (in the progress area) and as a toast. */
    private void showMessage(String msg) {
        if (progressText != null) {
            progressBar.setVisibility(View.GONE);
            progressText.setVisibility(View.VISIBLE);
            progressText.setText(msg);
        }
        Log.i(TAG, msg);
        toast(msg);
    }
}
