package com.ut2004.android;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.provider.DocumentsContract;
import android.provider.Settings;
import android.text.Editable;
import android.text.InputType;
import android.text.TextWatcher;
import android.util.Log;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.view.inputmethod.EditorInfo;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.Switch;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;
import java.util.Locale;

/**
 * Launcher: which game folder the game runs from, and getting the game into it.
 *
 * The engine reads its game folder by path. Internal storage (app-private /data, {@code getFilesDir()},
 * fast, deleted with the app) needs no permission: "Choose the UT2004 folder" opens Android's folder
 * picker, which gives the app that folder only, and "Copy to internal storage" copies it in (only what is
 * new or changed; with no folder chosen yet it opens the picker first). A UT2004 folder in the app's own
 * folder on a volume (Android/data/com.ut2004.android/files/UT2004) is read in place without permission
 * too; running in place from anywhere else on shared storage (/sdcard/UT2004, an SD card's UT2004) needs
 * All files access (Android 11 and later; the storage permission before that): starting from there
 * without it asks to allow it or to copy the game to internal storage instead. "Clear internal storage"
 * deletes the internal copy; "Copy settings and saves" writes the internal copy's inis and saves back to
 * the chosen folder.
 *
 * The storage choice is conveyed to the separate ":game" process via an Intent extra
 * ({@link GameActivity#EXTRA_STORAGE_MODE}); default-mode SharedPreferences would not be multi-process
 * safe, so they only keep the launcher's own choices. Start game also runs by itself after a countdown
 * that runs only while the page is in view and that the first touch, key or pad button stops.
 *
 * All widgets are built programmatically (the app has no androidx/Material dep and no XML layouts),
 * the same pattern as GameActivity's touch overlay.
 */
public class MainActivity extends Activity {
    private static final String TAG = "UT2004";
    private static final int COUNTDOWN_SECS = 10;
    private static final int REQ_STORAGE_PERMISSION = 1;
    private static final int REQ_PICK_FOLDER = 2;
    private static final String PREFS = "launcher";
    private static final String PREF_EXTERNAL = "runFromExternal";
    private static final String PREF_FOLDER = "folderUri";
    private static final String PREF_MOVE_FULL_TILT = "moveFullTiltDp";
    private static final String PREF_MOVE_DEAD_ZONE = "moveDeadZoneDp";
    // The game's own settings and saves, relative to the game folder.
    private static final String[] SETTINGS_AND_SAVES = { "System/UT2004.ini", "System/User.ini", "Save" };

    private boolean launcherShown = false;
    private boolean opRunning = false;
    private boolean gameLaunched = false;
    private boolean startWhenAllowed = false;  // back from the access request with access: start the game
    private boolean copyAfterPick = false;     // a folder picked for a copy: copy it

    private Switch storageSwitch;
    private Button startButton, pickButton, copyButton, clearButton, exportButton;
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

    private SharedPreferences prefs() { return getSharedPreferences(PREFS, MODE_PRIVATE); }

    @Override
    protected void onResume() {
        super.onResume();
        gameLaunched = false; // returning from the game (or first show) -- re-arm Start
        if (!launcherShown) {
            showLauncher();
        } else {
            // Returning from the game (Back), the picker or the settings screen -- refresh, but don't auto-relaunch.
            cancelCountdown(true);
            refreshStatusAsync();
            if (startWhenAllowed && Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                startWhenAllowed = false;
                if (hasAllFilesAccess()) launchGame();
            }
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        // The countdown runs only while the page is in view: not with the screen off or behind another app.
        cancelCountdown(true);
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

        // Storage picker: unchecked (left) = internal storage, checked (right) = the UT2004 folder in place.
        TextView pick = new TextView(this);
        pick.setText("Run the game from:");
        pick.setTextColor(0xFFB8BCC2);
        pick.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14f);
        pick.setPadding(0, dp(22), 0, dp(4));
        col.addView(pick);

        storageSwitch = new Switch(this);
        storageSwitch.setTextColor(0xFFECEFF3);
        storageSwitch.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16f);
        storageSwitch.setPadding(0, dp(2), 0, dp(2));
        storageSwitch.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override public void onCheckedChanged(CompoundButton btn, boolean checked) {
                applyStorageSwitchText();
                prefs().edit().putBoolean(PREF_EXTERNAL, checked).apply();
            }
        });
        col.addView(storageSwitch);

        statusText = new TextView(this);
        statusText.setTextColor(0xFF9AA0A8);
        statusText.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13f);
        statusText.setPadding(0, dp(16), 0, dp(4));
        statusText.setText("Checking storage…");
        col.addView(statusText);

        // The result of the last operation, under the status where it is seen without scrolling.
        progressText = new TextView(this);
        progressText.setTextColor(0xFFE8B34A);
        progressText.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13f);
        progressText.setPadding(0, dp(8), 0, dp(4));
        progressText.setVisibility(View.GONE);
        col.addView(progressText);

        progressBar = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        progressBar.setMax(100);
        progressBar.setVisibility(View.GONE);
        col.addView(progressBar, wide(dp(12)));

        pickButton = mkButton(col, "Choose the UT2004 folder…");
        pickButton.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { pickFolder(); }
        });

        copyButton = mkButton(col, "Copy to internal storage");
        copyButton.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { doCopyToInternal(); }
        });

        clearButton = mkButton(col, "Clear internal storage");
        clearButton.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { doClearInternal(); }
        });

        exportButton = mkButton(col, "Copy settings and saves to the UT2004 folder");
        exportButton.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { doExportSettings(); }
        });

        buildMoveStickRows(col);
        buildRendererRow(col);

        TextView hint = new TextView(this);
        hint.setTextColor(0xFF6E747C);
        hint.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11f);
        hint.setPadding(0, dp(26), 0, 0);
        hint.setText("Choose the UT2004 folder (on the device or an SD card) and copy it to internal storage for the "
                + "fastest loading. Playing from the UT2004 folder directly needs access to all files.");
        col.addView(hint);

        setContentView(scroll);
        // Out of touch mode, the first navigation key is consumed to leave it and never reaches
        // dispatchKeyEvent: leaving touch mode stops the countdown too.
        scroll.getViewTreeObserver().addOnTouchModeChangeListener(new ViewTreeObserver.OnTouchModeChangeListener() {
            @Override public void onTouchModeChanged(boolean inTouchMode) { if (!inTouchMode) cancelCountdown(true); }
        });

        // The last choice; the first time, internal storage when there is a copy there.
        storageSwitch.setChecked(prefs().getBoolean(PREF_EXTERNAL,
                !GameActivity.hasGameTree(GameActivity.internalRoot(this))));
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
        storageSwitch.setText(storageSwitch.isChecked() ? "UT2004 folder (slower)" : "Internal storage (fast)");
    }

    // ---- the touch move stick -------------------------------------------------------------------

    private NumberRow fullTiltRow, deadZoneRow;

    private void buildMoveStickRows(LinearLayout col) {
        label(col, "Touch move stick", 14f, 0xFFB8BCC2, 20);
        fullTiltRow = new NumberRow(col, "Full speed at a drag of (dp)", PREF_MOVE_FULL_TILT,
                GameActivity.MOVE_FULL_TILT_MIN_DP, GameActivity.MOVE_FULL_TILT_MAX_DP, 1f, GameActivity.MOVE_FULL_TILT_DP) {
            @Override String describe(float v) {
                return String.format(Locale.US, "Full speed %.1f mm from where the finger is held", v * 25.4f / 160f);
            }
            @Override void changed(float v) { if (deadZoneRow != null) deadZoneRow.refit(); }
        };
        deadZoneRow = new NumberRow(col, "Dead zone (dp)", PREF_MOVE_DEAD_ZONE,
                0f, GameActivity.MOVE_DEAD_ZONE_MAX_DP, 0.5f, GameActivity.MOVE_DEAD_ZONE_DP) {
            @Override float limit(float v) { return GameActivity.moveDeadZone(v, fullTiltRow.value()); }
            @Override String describe(float v) {
                return String.format(Locale.US, "A finger resting within %.1f mm does not move (always below full speed)", v * 25.4f / 160f);
            }
        };
    }

    /** The touch move stick's full tilt as saved, in its range. */
    private float savedMoveFullTilt() {
        return GameActivity.moveFullTilt(prefs().getFloat(PREF_MOVE_FULL_TILT, GameActivity.MOVE_FULL_TILT_DP));
    }

    /** The touch move stick's dead zone as saved, in its range and below full tilt. */
    private float savedMoveDeadZone() {
        return GameActivity.moveDeadZone(prefs().getFloat(PREF_MOVE_DEAD_ZONE, GameActivity.MOVE_DEAD_ZONE_DP), savedMoveFullTilt());
    }

    /**
     * A number setting: a slider over its steps, a field for the exact value and a line on what it does, saved as it
     * changes. limit() keeps a value inside bounds another setting sets; changed() tells the settings that depend on it.
     */
    private abstract class NumberRow {
        private final String pref;
        private final float min, max, step, def;
        private final SeekBar bar;
        private final EditText field;
        private final TextView note;
        private boolean updating = false;

        NumberRow(LinearLayout col, String title, String pref, float min, float max, float step, float def) {
            this.pref = pref;
            this.min = min;
            this.max = max;
            this.step = step;
            this.def = def;
            label(col, title, 13f, 0xFFB8BCC2, 10);
            LinearLayout row = new LinearLayout(MainActivity.this);
            row.setOrientation(LinearLayout.HORIZONTAL);
            row.setGravity(Gravity.CENTER_VERTICAL);
            col.addView(row, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
            bar = new SeekBar(MainActivity.this);
            bar.setMax(Math.round((max - min) / step));
            row.addView(bar, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
            field = new EditText(MainActivity.this);
            field.setInputType(InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_FLAG_DECIMAL);
            field.setImeOptions(EditorInfo.IME_ACTION_DONE);
            field.setSingleLine(true);
            field.setTextColor(0xFFECEFF3);
            field.setGravity(Gravity.CENTER);
            row.addView(field, new LinearLayout.LayoutParams(dp(88), ViewGroup.LayoutParams.WRAP_CONTENT));
            note = label(col, "", 12f, 0xFF9AA0A8, 2);
            show(value(), true, true);

            bar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
                @Override public void onProgressChanged(SeekBar b, int progress, boolean fromUser) {
                    if (fromUser && !updating) set(NumberRow.this.min + progress * NumberRow.this.step, false, true);
                }
                @Override public void onStartTrackingTouch(SeekBar b) { cancelCountdown(true); }
                @Override public void onStopTrackingTouch(SeekBar b) {}
            });
            field.addTextChangedListener(new TextWatcher() {
                @Override public void beforeTextChanged(CharSequence s, int a, int b, int c) {}
                @Override public void onTextChanged(CharSequence s, int a, int b, int c) {}
                @Override public void afterTextChanged(Editable s) {
                    if (updating) return;
                    float v;
                    try { v = Float.parseFloat(s.toString().trim()); } catch (NumberFormatException e) { v = -1f; }
                    if (v >= NumberRow.this.min && v <= NumberRow.this.max) set(v, true, false);
                    else note.setText(String.format(Locale.US, "Enter a value from %s to %s.",
                            format(NumberRow.this.min), format(NumberRow.this.max)));
                }
            });
        }

        float limit(float v) { return v; }
        abstract String describe(float v);
        void changed(float v) {}

        /** The saved value, in its range and its limit. */
        float value() { return limit(Math.max(min, Math.min(max, prefs().getFloat(pref, def)))); }

        /** Takes the saved value through limit() again: a setting it depends on moved. */
        void refit() {
            float v = value();
            prefs().edit().putFloat(pref, v).apply();
            show(v, true, true);
        }

        private void set(float v, boolean moveBar, boolean writeField) {
            float kept = limit(v);
            prefs().edit().putFloat(pref, kept).apply();
            show(kept, moveBar || kept != v, writeField || kept != v);
            changed(kept);
        }

        private void show(float v, boolean moveBar, boolean writeField) {
            updating = true;
            if (moveBar) bar.setProgress(Math.round((v - min) / step));
            if (writeField) field.setText(format(v));
            updating = false;
            note.setText(describe(v));
        }
    }

    private static String format(float v) {
        return v == Math.rint(v) ? String.format(Locale.US, "%.0f", v) : String.format(Locale.US, "%.1f", v);
    }

    private TextView label(LinearLayout col, String text, float sp, int color, int top) {
        TextView t = new TextView(this);
        t.setText(text);
        t.setTextColor(color);
        t.setTextSize(TypedValue.COMPLEX_UNIT_SP, sp);
        t.setPadding(0, dp(top), 0, dp(4));
        col.addView(t);
        return t;
    }

    // ---- start + countdown ----------------------------------------------------------------------

    private void startCountdown() {
        countdownActive = true;
        countdownRemaining = COUNTDOWN_SECS;
        startButton.setText("Start game (" + countdownRemaining + ")");
        ui.postDelayed(countdownTick, 1000L);
    }

    /** Stops a running countdown; relabel also clears a count left on the button by a countdown that ran out. */
    private void cancelCountdown(boolean relabel) {
        if (countdownActive) {
            countdownActive = false;
            ui.removeCallbacks(countdownTick);
        }
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

    /** So does the first key or pad button. */
    @Override
    public boolean dispatchKeyEvent(KeyEvent ev) {
        if (countdownActive && ev.getAction() == KeyEvent.ACTION_DOWN) cancelCountdown(true);
        return super.dispatchKeyEvent(ev);
    }

    private void launchGame() {
        if (opRunning || gameLaunched) return; // don't launch mid-copy, and never twice (countdown+tap race)
        String mode = storageSwitch.isChecked() ? GameActivity.MODE_EXTERNAL : GameActivity.MODE_INTERNAL;
        if (GameActivity.MODE_INTERNAL.equals(mode)) {
            // Internal selected but no local copy: tell the user rather than silently booting the UT2004
            // folder (resolveDataRoot still has that fallback as a defensive net for a cold recents relaunch).
            if (!GameActivity.hasGameTree(GameActivity.internalRoot(this))) {
                cancelCountdown(true);
                showMessage("There's no game in internal storage yet. Copy it from the UT2004 folder first.");
                return;
            }
        } else {
            File root = GameActivity.resolveExternalRoot(this);
            if (!GameActivity.hasGameTree(root)) {
                cancelCountdown(true);
                if (!hasAllFilesAccess()) askAllFilesAccess();
                else showMessage("No game folder at " + root + " (it needs System/Core.u).");
                return;
            }
        }
        gameLaunched = true;
        cancelCountdown(false);
        float fullTilt = savedMoveFullTilt(), deadZone = savedMoveDeadZone();
        Log.i(TAG, "launching game, storage=" + mode + ", move stick " + fullTilt + "/" + deadZone + " dp");
        Intent i = new Intent(this, GameActivity.class);
        i.putExtra(GameActivity.EXTRA_STORAGE_MODE, mode);
        i.putExtra(GameActivity.EXTRA_MOVE_FULL_TILT_DP, fullTilt);
        i.putExtra(GameActivity.EXTRA_MOVE_DEAD_ZONE_DP, deadZone);
        // Forward an extra engine command line (a travel URL) if this launcher was itself started
        // with one — see GameActivity.EXTRA_ARGS. GameActivity is not exported, so this hop is the
        // only route an `adb shell am start` has into the game process.
        String extraArgs = getIntent() != null
                ? getIntent().getStringExtra(GameActivity.EXTRA_ARGS) : null;
        // The renderer chosen below goes after them (an adb command line's own -*RENDERER switch comes first).
        String args = ((extraArgs != null ? extraArgs.trim() : "") + " " + prefs().getString(PREF_RENDERER, "")).trim();
        if (!args.isEmpty()) {
            i.putExtra(GameActivity.EXTRA_ARGS, args);
            Log.i(TAG, "forwarding engine arguments: " + args);
        }
        startActivity(i);
        // Deliberately not finish() -- Back from the game returns to this launcher.
    }

    // ---- access to the UT2004 folder in place ---------------------------------------------------

    /** All files access (Android 11 and later); the storage permission before that. */
    private boolean hasAllFilesAccess() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) return Environment.isExternalStorageManager();
        return checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED;
    }

    /**
     * Playing in place from the UT2004 folder needs All files access; copying the game to internal storage
     * does not. Allow access opens the system's page for it (the permission dialog before Android 11), and
     * the game starts when the player comes back with it on.
     */
    private void askAllFilesAccess() {
        new AlertDialog.Builder(this)
                .setTitle("Allow access to all files?")
                .setMessage("To play from the UT2004 folder, UT2004 needs access to all files. Or copy the game to "
                        + "internal storage — no access needed.")
                .setNeutralButton("Copy to internal storage", new DialogInterface.OnClickListener() {
                    @Override public void onClick(DialogInterface d, int w) { doCopyToInternal(); }
                })
                .setPositiveButton("Allow access", new DialogInterface.OnClickListener() {
                    @Override public void onClick(DialogInterface d, int w) { requestAllFilesAccess(); }
                })
                .setNegativeButton("Cancel", null)
                .show();
    }

    private void requestAllFilesAccess() {
        startWhenAllowed = true;
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            requestPermissions(new String[] {
                    Manifest.permission.READ_EXTERNAL_STORAGE,
                    Manifest.permission.WRITE_EXTERNAL_STORAGE }, REQ_STORAGE_PERMISSION);
            return;
        }
        try {
            startActivity(new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                    Uri.parse("package:" + getPackageName())));
        } catch (Exception e) {
            Log.e(TAG, "per-app All-files-access settings unavailable; opening the global list", e);
            startActivity(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION));
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode != REQ_STORAGE_PERMISSION) return;
        refreshStatusAsync();
        if (startWhenAllowed) {
            startWhenAllowed = false;
            if (hasAllFilesAccess()) launchGame();
        }
    }

    // ---- the folder picker ----------------------------------------------------------------------

    private Uri chosenFolder() {
        String s = prefs().getString(PREF_FOLDER, null);
        return s == null ? null : Uri.parse(s);
    }

    private void pickFolder() {
        cancelCountdown(true);
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION
                | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        // Opens at /sdcard/UT2004 when it exists.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            i.putExtra(DocumentsContract.EXTRA_INITIAL_URI,
                    DocumentsContract.buildDocumentUri("com.android.externalstorage.documents", "primary:UT2004"));
        }
        try {
            startActivityForResult(i, REQ_PICK_FOLDER);
        } catch (Exception e) {
            copyAfterPick = false;
            showMessage("This device has no folder picker.");
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != REQ_PICK_FOLDER) return;
        final boolean copy = copyAfterPick;
        copyAfterPick = false;
        if (resultCode != RESULT_OK || data == null || data.getData() == null) return;
        final Uri tree = data.getData();
        try {
            getContentResolver().takePersistableUriPermission(tree,
                    Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        } catch (SecurityException e) {
            Log.w(TAG, "no persistable permission for " + tree, e);
        }
        prefs().edit().putString(PREF_FOLDER, tree.toString()).apply();
        Log.i(TAG, "chosen folder " + tree);
        new Thread(new Runnable() {
            @Override public void run() {
                final boolean ok = Storage.isGameTree(getContentResolver(), tree);
                ui.post(new Runnable() { @Override public void run() {
                    if (!ok) showMessage("That folder doesn't contain the game (no System/Core.u). Choose the UT2004 folder.");
                    else if (copy) doCopyToInternal();
                    else showMessage("UT2004 folder chosen: " + folderName(tree));
                    refreshStatusAsync();
                } });
            }
        }, "ut2004-check").start();
    }

    /** "UT2004 (shared storage)", "UT2004 (SD card 3434-D61F)": a tree's document id is volume:path. */
    private static String folderName(Uri tree) {
        String id = DocumentsContract.getTreeDocumentId(tree);
        int colon = id.indexOf(':');
        String vol = colon > 0 ? id.substring(0, colon) : "";
        String path = colon >= 0 ? id.substring(colon + 1) : id;
        return (path.isEmpty() ? "/" : path)
                + ("primary".equals(vol) ? " (shared storage)" : vol.isEmpty() ? "" : " (SD card " + vol + ")");
    }

    // ---- storage operations ---------------------------------------------------------------------

    private void doCopyToInternal() {
        if (opRunning) return;
        cancelCountdown(true);
        final File dst = GameActivity.internalRoot(this);
        final Uri tree = chosenFolder();
        final File src = GameActivity.resolveExternalRoot(this);
        if (tree == null && !GameActivity.hasGameTree(src)) {
            // No folder yet: choose it, and the copy follows.
            copyAfterPick = true;
            pickFolder();
            return;
        }
        startOp("Copying to internal storage…");
        new Thread(new Runnable() {
            @Override public void run() {
                String err;
                if (tree != null) {
                    err = Storage.isGameTree(getContentResolver(), tree)
                            ? Storage.syncFromTree(getContentResolver(), tree, dst, throttledListener())
                            : "UT2004 can't read the chosen folder anymore. Choose the UT2004 folder again.";
                } else {
                    err = Storage.syncTree(src, dst, throttledListener());
                }
                final String error = err;
                ui.post(new Runnable() { @Override public void run() {
                    // A finished copy is what the game runs from.
                    if (error == null) storageSwitch.setChecked(false);
                    finishOp(error == null ? "Copy complete." : error);
                } });
            }
        }, "ut2004-copy").start();
    }

    private void doClearInternal() {
        if (opRunning) return;
        cancelCountdown(true);
        final File dst = GameActivity.internalRoot(this);
        if (!dst.exists()) { showMessage("Internal storage has no game."); return; }
        new AlertDialog.Builder(this)
                .setTitle("Clear internal storage?")
                .setMessage("Delete the internal copy at\n" + dst + "\n(" + Storage.human(Storage.dirSize(dst))
                        + "), its settings and saves included?")
                .setPositiveButton("Delete", new DialogInterface.OnClickListener() {
                    @Override public void onClick(DialogInterface d, int w) {
                        startOp("Clearing internal storage…");
                        new Thread(new Runnable() {
                            @Override public void run() {
                                final boolean ok = Storage.deleteTree(dst);
                                final String msg = ok ? "Internal storage cleared." : "Some files could not be deleted.";
                                ui.post(new Runnable() { @Override public void run() { finishOp(msg); } });
                            }
                        }, "ut2004-clear").start();
                    }
                })
                .setNegativeButton("Cancel", null)
                .show();
    }

    private void doExportSettings() {
        if (opRunning) return;
        cancelCountdown(true);
        final File internal = GameActivity.internalRoot(this);
        final Uri tree = chosenFolder();
        final File external = GameActivity.resolveExternalRoot(this);
        if (!GameActivity.hasGameTree(internal)) {
            showMessage("Internal storage has no game to copy settings from.");
            return;
        }
        if (tree == null && !GameActivity.hasGameTree(external)) {
            showMessage("Choose the UT2004 folder first.");
            return;
        }
        startOp("Copying settings and saves…");
        new Thread(new Runnable() {
            @Override public void run() {
                StringBuilder err = new StringBuilder();
                int files = 0;
                if (tree != null) {
                    files = Storage.exportToTree(getContentResolver(), tree, internal, SETTINGS_AND_SAVES, err);
                } else {
                    for (String rel : SETTINGS_AND_SAVES) {
                        File s = new File(internal, rel);
                        String e = null;
                        if (s.isDirectory()) e = Storage.syncTree(s, new File(external, rel), null);
                        else if (s.isFile()) e = Storage.copyFileInto(s, new File(external, rel));
                        else continue;
                        if (e != null) err.append(rel).append(": ").append(e).append('\n');
                        else files++;
                    }
                }
                final String msg = err.length() > 0 ? "Copied with errors:\n" + err
                        : "Copied the settings and saves (" + files + ") to "
                            + (tree != null ? folderName(tree) : external.toString());
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
        pickButton.setEnabled(enabled);
        copyButton.setEnabled(enabled);
        clearButton.setEnabled(enabled);
        exportButton.setEnabled(enabled);
        storageSwitch.setEnabled(enabled);
    }

    // ---- status readout -------------------------------------------------------------------------

    private void refreshStatusAsync() {
        if (statusText == null) return;
        final Uri tree = chosenFolder();
        new Thread(new Runnable() {
            @Override public void run() {
                File internal = GameActivity.internalRoot(MainActivity.this);
                File external = GameActivity.resolveExternalRoot(MainActivity.this);
                long free = Storage.freeBytes(getFilesDir());
                boolean intReady = GameActivity.hasGameTree(internal);
                boolean extReady = GameActivity.hasGameTree(external);
                String chosen = tree == null ? "none" : folderName(tree)
                        + (Storage.isGameTree(getContentResolver(), tree) ? "" : "  (no System/Core.u, or no access)");
                String inPlace = extReady ? Storage.human(Storage.dirSize(external)) + "  " + external
                        : hasAllFilesAccess() ? "no game folder at " + external
                        : "needs access to all files";
                final String s = "Internal storage free: " + Storage.human(free) + '\n'
                        + "Internal copy: " + (intReady ? Storage.human(Storage.dirSize(internal)) + "  " + internal : "none") + '\n'
                        + "Chosen UT2004 folder: " + chosen + '\n'
                        + "UT2004 folder, in place: " + inPlace;
                ui.post(new Runnable() { @Override public void run() { statusText.setText(s); } });
            }
        }, "ut2004-status").start();
    }

    /** Surface a message both persistently (in the progress area) and as a toast. */
    private void showMessage(String msg) {
        if (progressText != null) {
            progressBar.setVisibility(View.GONE);
            progressText.setVisibility(View.VISIBLE);
            progressText.setText(msg);
        }
        Log.i(TAG, msg);
        Toast.makeText(this, msg, Toast.LENGTH_LONG).show();
    }

    // ---- the renderer ---------------------------------------------------------------------------

    private static final String PREF_RENDERER = "renderer";

    /**
     * The render devices of this build, as the engine's -*RENDERER switches. The first, no switch, leaves the choice
     * to the game: its default, or the device it last started with (a switch is remembered by the game).
     */
    private static final String[][] RENDERERS = {
            { "", "The game's choice" },
            { "-GL4ESRENDERER", "GL4ES (OpenGL ES through gl4es)" },
            { "-NULLRENDERER", "Null (draws nothing: for debugging)" }
    };

    private void buildRendererRow(LinearLayout col) {
        label(col, "Renderer", 14f, 0xFFB8BCC2, 20);
        RadioGroup group = new RadioGroup(this);
        String current = prefs().getString(PREF_RENDERER, "");
        int checked = -1;
        for (int k = 0; k < RENDERERS.length; k++) {
            RadioButton b = new RadioButton(this);
            b.setId(View.generateViewId());
            b.setText(RENDERERS[k][1]);
            b.setTextColor(0xFFECEFF3);
            b.setTag(RENDERERS[k][0]);
            group.addView(b);
            if (RENDERERS[k][0].equals(current)) checked = b.getId();
        }
        if (checked != -1) group.check(checked);
        group.setOnCheckedChangeListener(new RadioGroup.OnCheckedChangeListener() {
            @Override public void onCheckedChanged(RadioGroup g, int id) {
                cancelCountdown(true);
                View b = g.findViewById(id);
                if (b != null) prefs().edit().putString(PREF_RENDERER, (String) b.getTag()).apply();
            }
        });
        col.addView(group);
        label(col, "For a device whose GPU draws the game wrong with the usual renderer. The game keeps the renderer "
                + "it last started with, also with \"The game's choice\".", 12f, 0xFF9AA0A8, 2);
    }
}
