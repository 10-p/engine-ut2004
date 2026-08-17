package com.ut2004.android;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.os.Bundle;
import android.os.Environment;
import android.os.SystemClock;
import android.util.Log;
import android.util.SparseArray;
import android.view.MotionEvent;
import android.view.View;
import android.widget.FrameLayout;

import java.io.File;
import java.io.InputStream;

import org.libsdl.app.SDLActivity;

/**
 * UT2004 game process (SDL). MainActivity gates All-Files-Access, then launches this in the
 * ":game" process. Before SDL starts SDL_main we set the engine's basedir env + cwd so it reads
 * the ~11 GB game tree IN PLACE from /sdcard/UT2004 (no copy).
 *
 * Touch input (2.10d): after SDL builds its surface we lay a full-screen {@link Ut2004TouchOverlayView}
 * on top. It draws the on-screen controls and, during gameplay, feeds the engine through the JNI
 * funcs below (implemented in SDL2Drv/Src/SDL2Viewport.cpp). While a menu is up it disables itself
 * and lets touches fall through to SDL's touch->mouse tap-to-click. The switch is the engine's own
 * mouse state, published via {@link #nativeIsMenuVisible()}.
 */
public class GameActivity extends SDLActivity {
    private static final String TAG = "UT2004";
    private static boolean bridgeLoaded = false;

    // Storage-mode selection (Stage 2.11). MainActivity's launcher puts one of these on the launch
    // Intent; we read it in resolveDataRoot() (before super.onCreate) to pick which tree to boot.
    //   internal = app-private /data (getFilesDir()/UT2004) -- fast, non-FUSE, but wiped on uninstall.
    //   external = the slow FUSE tree (/sdcard/UT2004 or a physical SD card) -- the complete game.
    static final String EXTRA_STORAGE_MODE = "com.ut2004.android.STORAGE_MODE";
    static final String MODE_INTERNAL = "internal";
    static final String MODE_EXTERNAL = "external";

    /** App-private fast game root: {@code getFilesDir()/UT2004} (same path in both processes). */
    static File internalRoot(Context ctx) { return new File(ctx.getFilesDir(), "UT2004"); }

    /** A dir is a usable game root iff it has a {@code System/} subdir (where the engine boots from). */
    static boolean hasGameTree(File root) {
        return root != null && new File(root, "System").isDirectory();
    }

    static {
        // Load the tiny process-prep bridge in a static block (independent of SDL's own loader)
        // so nativePrepareProcess is resolvable the moment onCreate runs.
        try {
            System.loadLibrary("ut2004_android_bridge");
            bridgeLoaded = true;
        } catch (Throwable t) {
            Log.e(TAG, "failed to load ut2004_android_bridge", t);
        }
    }

    // android_bridge.cpp: setenv(UT2004_ANDROID_BASEDIR=<dataRoot>/System) + chdir there.
    private static native boolean nativePrepareProcess(String dataRoot, String homeDir);

    // Touch overlay <-> engine bridge (SDL2Viewport.cpp, __ANDROID__ guarded).
    // Resolvable once libUT2004.so is loaded via getLibraries().
    private static native boolean nativeIsMenuVisible();
    private static native void nativeTouchAxis(int axis, float value);   // axis 0=strafe, 1=forward
    private static native void nativeTouchLook(float dx, float dy);      // raw pixel drag deltas
    private static native void nativeTouchButton(int button, boolean down);

    // Semantic button ids -- keep in sync with EAndroidTouchButton in SDL2Viewport.cpp.
    private static final int BTN_FIRE = 0, BTN_ALTFIRE = 1, BTN_JUMP = 2, BTN_CROUCH = 3,
            BTN_NEXTWEAPON = 4, BTN_MENU = 5;

    private Ut2004TouchOverlayView touchOverlay;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // MUST run BEFORE super.onCreate() — super.onCreate() spins up the SDL thread that calls
        // SDL_main, and the engine's appBaseDir()/appInit() read the env + cwd we set here.
        File dataRoot = resolveDataRoot();
        File homeDir = new File(getFilesDir(), "home");
        if (bridgeLoaded) {
            boolean ok = nativePrepareProcess(dataRoot.getAbsolutePath(), homeDir.getAbsolutePath());
            Log.i(TAG, "nativePrepareProcess(dataRoot=" + dataRoot + ") -> " + ok);
        } else {
            Log.e(TAG, "bridge not loaded; the engine will not find its data at " + dataRoot);
        }
        super.onCreate(savedInstanceState);

        // Keep the screen awake during play (2.10f lifecycle): a match can run for long stretches
        // with no touch input (spectating, watching bots), and the default screen timeout would
        // dim/sleep the display. FLAG_KEEP_SCREEN_ON while this activity is foreground; cleared
        // automatically when it isn't. Matches the UTA reference. (Background suspend is free --
        // stock SDL parks the engine thread on pause; the resume delta is clamped by ULevel::Tick.)
        getWindow().addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        // Lay the touch controls on top of SDL's surface (mLayout is the content view).
        try {
            touchOverlay = new Ut2004TouchOverlayView(this);
            addContentView(touchOverlay, new FrameLayout.LayoutParams(
                    FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));
            Log.i(TAG, "touch overlay installed");
        } catch (Throwable t) {
            Log.e(TAG, "touch overlay install failed", t);
        }
    }

    /**
     * Locate the UT2004 game tree, honoring the Stage 2.11 storage choice on the launch Intent
     * ({@link #EXTRA_STORAGE_MODE}, read before super.onCreate):
     *   - {@code internal} -> app-private {@link #internalRoot} if it holds a game tree, else fall
     *     through to the external auto-detect (graceful: an empty internal copy still boots off the card).
     *   - {@code external} -> {@link #resolveExternalRoot} (the complete, slow FUSE tree).
     *   - no extra (a cold recents relaunch) -> prefer a populated internal copy, else auto-detect.
     * Requires storage permission, which MainActivity has already ensured before starting this activity.
     */
    private File resolveDataRoot() {
        String mode = getIntent() != null ? getIntent().getStringExtra(EXTRA_STORAGE_MODE) : null;
        File internal = internalRoot(this);
        if (MODE_INTERNAL.equals(mode)) {
            if (hasGameTree(internal)) {
                Log.i(TAG, "data root (internal /data, requested): " + internal);
                return internal;
            }
            Log.w(TAG, "internal storage requested but empty (" + internal + "); using external");
            return resolveExternalRoot(this);
        }
        if (MODE_EXTERNAL.equals(mode)) {
            return resolveExternalRoot(this);
        }
        // No extra (cold recents launch of the game process): prefer a populated internal copy.
        if (hasGameTree(internal)) {
            Log.i(TAG, "data root (internal /data, recents default): " + internal);
            return internal;
        }
        return resolveExternalRoot(this);
    }

    /**
     * The external (slow FUSE) game tree. Prefer internal primary storage (/sdcard/UT2004 -- emulator
     * + phones with room), else scan secondary volumes (a physical SD card, e.g. on the Retroid where
     * internal is too small): derive each volume root from the app's external-files dirs and look for
     * &lt;volume&gt;/UT2004 (and the app-specific &lt;...&gt;/files/UT2004). Static so MainActivity's
     * launcher uses the exact same source for copy-to-internal and the external-size readout.
     */
    static File resolveExternalRoot(Context ctx) {
        File primary = new File(Environment.getExternalStorageDirectory(), "UT2004");
        if (hasGameTree(primary)) {
            Log.i(TAG, "external root (internal primary): " + primary);
            return primary;
        }
        try {
            for (File appDir : ctx.getExternalFilesDirs(null)) {
                if (appDir == null) continue;
                // appDir = <volume>/Android/data/<pkg>/files ; the volume root is 4 levels up.
                File vol = appDir;
                for (int i = 0; i < 4 && vol != null; i++) vol = vol.getParentFile();
                if (vol != null) {
                    File atRoot = new File(vol, "UT2004");
                    if (hasGameTree(atRoot)) {
                        Log.i(TAG, "external root (secondary volume): " + atRoot);
                        return atRoot;
                    }
                }
                File inApp = new File(appDir, "UT2004");
                if (hasGameTree(inApp)) {
                    Log.i(TAG, "external root (app-specific dir): " + inApp);
                    return inApp;
                }
            }
        } catch (Throwable t) {
            Log.w(TAG, "secondary-volume data-root scan failed", t);
        }
        Log.w(TAG, "no UT2004/System found on any volume; defaulting to " + primary);
        return primary;
    }

    @Override
    protected String[] getLibraries() {
        return new String[] {
            "SDL2",
            "UT2004",
        };
    }

    @Override
    protected String[] getArguments() {
        // Engine writes (UT2004.log, UT2004.ini, User.ini, config flush) land in the cwd
        // (<dataRoot>/System) instead of ~/.ut2004 — everything stays on external storage. That used
        // to need an explicit "-nohomedir"; since ufront 2.37 it is the engine's default and the flag
        // is an accepted no-op, so it is gone from here. "-usehomedir" would opt back in; nothing on
        // Android wants that ($HOME is the app shell's, set in the JNI bridge).
        //
        // CPUSPEED=3000 (2.10 arm boot): ARM has no rdtsc and /proc/cpuinfo has no "cpu MHz", so
        // Core's CheckCpuSpeed (UnUnix.cpp, the UT_NO_X86_INTRIN path) fatally appErrors
        // ("Please specify CPUSPEED=x on cmdline.") without it — this is what kills the arm64/armv7
        // boot right after "Object subsystem initialized". The web port hits the same wasm path and
        // passes the identical CPUSPEED=3000 (packages/frontend shell/boot.ts). GSecondsPerCycle only
        // scales appCycles()->seconds for profiling; the game loop uses appSeconds wall-clock, so the
        // exact value is non-critical — 3000 matches web.
        return new String[] { "CPUSPEED=3000" };
    }

    private static float clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

    /**
     * Full-screen touch controls, ported from the UTA (UT99-Android) overlay and rewired for UT2004.
     * Left half = analog move stick; right half = relative drag-look; circular FIRE/ALTFIRE/JUMP/
     * CROUCH/NEXT/MENU buttons. A second free touch on the right half becomes Fire while the first
     * keeps aiming. Disables itself (and passes touches through to SDL) while a menu is up.
     */
    private static final class Ut2004TouchOverlayView extends View {
        private final GameActivity activity;
        private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final RectF rect = new RectF();
        private final SparseArray<TouchRole> roles = new SparseArray<TouchRole>();
        private final Bitmap iconFire, iconAltFire, iconJump, iconCrouch, iconNext, iconMenu;

        private long lastMenuReadMs;
        private boolean menuVisible = false;
        private float leftBaseX, leftBaseY, rightLastX, rightLastY, lx, ly;
        private boolean fireButton, altFire, jump, crouch, next, menu, fire;
        private int rightFireAssistCount = 0;

        private enum TouchRole { LEFT_STICK, RIGHT_LOOK, RIGHT_FIRE_ASSIST, FIRE, ALTFIRE, JUMP, CROUCH, NEXT, MENU }

        Ut2004TouchOverlayView(GameActivity activity) {
            super(activity);
            this.activity = activity;
            setWillNotDraw(false);
            setFocusable(false);
            iconFire = loadIcon("touch_overlay/fire.png");
            iconAltFire = loadIcon("touch_overlay/alternate-fire.png");
            iconJump = loadIcon("touch_overlay/jump.png");
            iconCrouch = loadIcon("touch_overlay/crouch.png");
            iconNext = loadIcon("touch_overlay/next-weapon.png");
            iconMenu = loadIcon("touch_overlay/menu.png");
            postDelayed(redrawRunnable, 66L);
        }

        // Periodic invalidate so the overlay shows/hides promptly when menu state flips.
        private final Runnable redrawRunnable = new Runnable() {
            @Override public void run() { invalidate(); postDelayed(this, 66L); }
        };

        private Bitmap loadIcon(String assetPath) {
            try {
                InputStream in = activity.getAssets().open(assetPath);
                try { return BitmapFactory.decodeStream(in); }
                finally { in.close(); }
            } catch (Throwable t) {
                Log.w(TAG, "touch overlay missing icon " + assetPath, t);
                return null;
            }
        }

        private void refreshMenuState(boolean force) {
            long now = SystemClock.uptimeMillis();
            if (force || now - lastMenuReadMs > 120L) {
                lastMenuReadMs = now;
                try { menuVisible = nativeIsMenuVisible(); } catch (Throwable t) { menuVisible = false; }
            }
        }

        private boolean active(boolean force) { refreshMenuState(force); return !menuVisible; }

        @Override protected void onDraw(Canvas canvas) {
            super.onDraw(canvas);
            refreshMenuState(false);
            if (menuVisible || canvas == null) return;
            float w = getWidth(), h = getHeight();
            if (w <= 0 || h <= 0) return;
            float s = Math.min(w, h);
            float pad = Math.max(10f, s * 0.020f);
            float r = Math.max(50f, s * 0.0705f);
            float gap = Math.max(12f, s * 0.018f);

            drawIconButton(canvas, pad + r * 0.72f, pad + r * 0.72f, r * 0.72f, iconMenu);
            float actionY1 = h * 0.42f + r * 2.0f;
            float actionY2 = actionY1 + r * 2.0f + gap;
            float nextY = actionY1 - r * 2.0f - gap;
            drawIconButton(canvas, w - pad - r, nextY, r, iconNext);
            drawIconButton(canvas, w - pad - r, actionY1, r, iconFire);
            drawIconButton(canvas, w - pad - r, actionY2, r, iconAltFire);

            float bottomY = h - pad - r;
            float bottomShiftLeft = r;
            drawIconButton(canvas, w - pad - r - bottomShiftLeft, bottomY, r, iconCrouch);
            drawIconButton(canvas, w - pad - r * 3.05f - gap - bottomShiftLeft, bottomY, r, iconJump);
        }

        private void drawIconButton(Canvas canvas, float cx, float cy, float r, Bitmap icon) {
            paint.setStyle(Paint.Style.FILL);
            paint.setColor(0x06202020);
            canvas.drawCircle(cx, cy, r, paint);
            paint.setStyle(Paint.Style.STROKE);
            paint.setStrokeWidth(Math.max(2f, r * 0.055f));
            paint.setColor(0x1AFFFFFF);
            canvas.drawCircle(cx, cy, r, paint);
            if (icon != null) {
                float iconR = r * 0.64f;
                rect.set(cx - iconR, cy - iconR, cx + iconR, cy + iconR);
                paint.setStyle(Paint.Style.FILL);
                paint.setAlpha(40);
                canvas.drawBitmap(icon, null, rect, paint);
                paint.setAlpha(255);
            }
        }

        private boolean hasActiveRightLookPointer() {
            for (int i = 0; i < roles.size(); ++i)
                if (roles.valueAt(i) == TouchRole.RIGHT_LOOK) return true;
            return false;
        }

        private TouchRole resolveRole(float x, float y) {
            TouchRole role = hitRole(x, y);
            // Second free touch on the right half = Fire while the original pointer keeps aiming.
            if (role == TouchRole.RIGHT_LOOK && x >= getWidth() * 0.5f && hasActiveRightLookPointer())
                return TouchRole.RIGHT_FIRE_ASSIST;
            return role;
        }

        private TouchRole hitRole(float x, float y) {
            float w = getWidth(), h = getHeight(), s = Math.min(w, h);
            float pad = Math.max(10f, s * 0.020f), r = Math.max(50f, s * 0.0705f), gap = Math.max(12f, s * 0.018f);
            if (insideCircle(x, y, pad + r * 0.72f, pad + r * 0.72f, r * 1.10f)) return TouchRole.MENU;
            float actionY1 = h * 0.42f + r * 2.0f, actionY2 = actionY1 + r * 2.0f + gap, nextY = actionY1 - r * 2.0f - gap;
            if (insideCircle(x, y, w - pad - r, nextY, r * 1.35f)) return TouchRole.NEXT;
            if (insideCircle(x, y, w - pad - r, actionY1, r * 1.35f)) return TouchRole.FIRE;
            if (insideCircle(x, y, w - pad - r, actionY2, r * 1.35f)) return TouchRole.ALTFIRE;
            float bottomY = h - pad - r, bottomShiftLeft = r;
            if (insideCircle(x, y, w - pad - r - bottomShiftLeft, bottomY, r * 1.35f)) return TouchRole.CROUCH;
            if (insideCircle(x, y, w - pad - r * 3.05f - gap - bottomShiftLeft, bottomY, r * 1.35f)) return TouchRole.JUMP;
            return x < w * 0.5f ? TouchRole.LEFT_STICK : TouchRole.RIGHT_LOOK;
        }

        private boolean insideCircle(float x, float y, float cx, float cy, float r) {
            float dx = x - cx, dy = y - cy;
            return dx * dx + dy * dy <= r * r;
        }

        private float analogValue(float delta, float radius, float dead, float scale) {
            float v = clamp(delta / radius, -1f, 1f);
            if (Math.abs(v) < dead) return 0f;
            v = (v > 0f ? (v - dead) : (v + dead)) / (1f - dead);
            return clamp(v * scale, -1f, 1f);
        }

        @Override public boolean onTouchEvent(MotionEvent event) {
            if (event == null) return false;
            int action = event.getActionMasked(), index = event.getActionIndex();
            int pointerId = (index >= 0 && index < event.getPointerCount()) ? event.getPointerId(index) : -1;
            TouchRole existingRole = pointerId >= 0 ? roles.get(pointerId) : null;

            if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN) {
                // Fresh native menu query per new pointer; never consume a menu tap (pass to SDL).
                if (!active(true)) { releaseAll(); roles.clear(); return false; }
                TouchRole role = resolveRole(event.getX(index), event.getY(index));
                roles.put(pointerId, role);
                if (role == TouchRole.LEFT_STICK) { leftBaseX = event.getX(index); leftBaseY = event.getY(index); }
                else if (role == TouchRole.RIGHT_LOOK) { rightLastX = event.getX(index); rightLastY = event.getY(index); }
                if (role == TouchRole.RIGHT_FIRE_ASSIST) addRightFireAssist();
                else updateRole(role, event.getX(index), event.getY(index), true);
                return true;
            }
            if (action == MotionEvent.ACTION_MOVE) {
                if (roles.size() == 0 && !active(true)) return false;
                for (int i = 0; i < event.getPointerCount(); ++i) {
                    TouchRole role = roles.get(event.getPointerId(i));
                    if (role != null && role != TouchRole.RIGHT_FIRE_ASSIST)
                        updateRole(role, event.getX(i), event.getY(i), true);
                }
                return true;
            }
            if (action == MotionEvent.ACTION_CANCEL) { releaseAll(); roles.clear(); return true; }
            if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_POINTER_UP) {
                if (existingRole != null) {
                    if (existingRole == TouchRole.RIGHT_FIRE_ASSIST) removeRightFireAssist();
                    else updateRole(existingRole, event.getX(index), event.getY(index), false);
                    roles.remove(pointerId);
                    return true;
                }
                if (!active(true)) return false;
            }
            return roles.size() > 0;
        }

        private void syncFire() {
            boolean want = fireButton || rightFireAssistCount > 0;
            if (fire != want) { fire = want; nativeTouchButton(BTN_FIRE, want); }
        }
        private void addRightFireAssist() { rightFireAssistCount++; syncFire(); }
        private void removeRightFireAssist() { if (rightFireAssistCount > 0) rightFireAssistCount--; syncFire(); }

        private void updateRole(TouchRole role, float x, float y, boolean down) {
            float s = Math.min(getWidth(), getHeight());
            float moveRadius = Math.max(112f, s * 0.145f);
            switch (role) {
                case LEFT_STICK:
                    lx = down ? analogValue(x - leftBaseX, moveRadius, 0.08f, 1.0f) : 0f;
                    ly = down ? analogValue(y - leftBaseY, moveRadius, 0.08f, 1.0f) : 0f;
                    nativeTouchAxis(0, lx);
                    nativeTouchAxis(1, ly);
                    break;
                case RIGHT_LOOK:
                    if (down) {
                        float dx = x - rightLastX, dy = y - rightLastY;
                        rightLastX = x; rightLastY = y;
                        // Raw pixel deltas -> mouse-look (native folds them onto IK_MouseX/Y).
                        if (dx != 0f || dy != 0f) nativeTouchLook(dx, dy);
                    }
                    break;
                case RIGHT_FIRE_ASSIST: break;
                case FIRE:    setFireButton(down); break;
                case ALTFIRE: if (altFire != down) { altFire = down; nativeTouchButton(BTN_ALTFIRE, down); } break;
                case JUMP:    if (jump != down)    { jump = down;    nativeTouchButton(BTN_JUMP, down); } break;
                case CROUCH:  if (crouch != down)  { crouch = down;  nativeTouchButton(BTN_CROUCH, down); } break;
                case NEXT:    if (next != down)    { next = down;    if (down) nativeTouchButton(BTN_NEXTWEAPON, true); } break;
                case MENU:    if (menu != down)    { menu = down;    nativeTouchButton(BTN_MENU, down); } break;
            }
        }

        private void setFireButton(boolean down) {
            if (fireButton != down) { fireButton = down; syncFire(); }
        }

        private void releaseAll() {
            if (lx != 0f || ly != 0f) { lx = ly = 0f; nativeTouchAxis(0, 0f); nativeTouchAxis(1, 0f); }
            if (fire || fireButton || rightFireAssistCount > 0) {
                fire = false; fireButton = false; rightFireAssistCount = 0; nativeTouchButton(BTN_FIRE, false);
            }
            if (altFire) { altFire = false; nativeTouchButton(BTN_ALTFIRE, false); }
            if (jump)    { jump = false;    nativeTouchButton(BTN_JUMP, false); }
            if (crouch)  { crouch = false;  nativeTouchButton(BTN_CROUCH, false); }
            if (menu)    { menu = false;    nativeTouchButton(BTN_MENU, false); }
            next = false;
        }
    }
}
