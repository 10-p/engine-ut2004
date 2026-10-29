package com.ut2004.android;

import android.app.Activity;
import android.content.Intent;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.util.Log;
import android.view.Display;
import android.view.Surface;

import java.util.Locale;

/**
 * Gyro aiming: the device's rotation turns the view by the device's own angle times the launcher's sensitivities (view
 * degrees per device degree, so at 1 the view turns as far as the device), with nothing in between: no smoothing, no
 * acceleration, no dead zone. The engine converts the degrees to its own rotation units and applies them in play only
 * ({@link GameActivity#nativeGyroLook}).
 *
 * The mapping is in the screen's frame (x to the right, y up, z out of the screen), whichever way the display is
 * rotated. Up and down is the turn about the screen's x axis: tilting the top edge towards the player looks up, as
 * through a window. Left and right is "player space" yaw: the turn about the world's vertical, taken from the turns
 * about the screen's y axis (turning the device) and z axis (twisting it like a wheel) weighted by where gravity
 * points, so turning the device left turns the view left however the device is tilted, and with the device tilted
 * back 45 degrees or more a twist turns the view as far as a turn does. Its size is never more than the device's
 * combined turn and twist.
 */
final class GyroAim implements SensorEventListener {
    private static final String TAG = "UT2004";

    /** The launcher's settings: whether gyro aiming is on, and the two sensitivities. Absent: off, 1 and 1. */
    static final String EXTRA_GYRO_AIM = "com.ut2004.android.GYRO_AIM";
    static final String EXTRA_GYRO_YAW_SCALE = "com.ut2004.android.GYRO_YAW_SCALE";
    static final String EXTRA_GYRO_PITCH_SCALE = "com.ut2004.android.GYRO_PITCH_SCALE";
    static final float SCALE_MIN = 0.1f, SCALE_MAX = 5f, SCALE_DEFAULT = 1f;

    static float scale(float v) {
        return Math.max(SCALE_MIN, Math.min(SCALE_MAX, v));
    }

    // Player space: the world yaw counts this many times over, up to the device's turn and twist (1 / cos 45 degrees).
    private static final float YAW_RELAX = 1.4142135f;
    private static final float DEGREES_PER_RADIAN = (float) (180.0 / Math.PI);

    private final Display display;
    private final SensorManager sensors;
    private final Sensor gyroscope, gravity;
    private final float yawScale, pitchScale;
    // Where up is (gravity's opposite) in the device's frame; until a reading arrives, the screen's frame of a device
    // tilted back 45 degrees.
    private final float[] up = { 0f, 0.7071f, 0.7071f };
    private boolean upInScreen = true;
    private long lastNanos = 0L;
    private boolean listening = false;

    /** Gyro aiming as the launcher set it, or null when it is off. */
    static GyroAim fromIntent(Activity activity) {
        Intent in = activity.getIntent();
        if (in == null || !in.getBooleanExtra(EXTRA_GYRO_AIM, false)) {
            Log.i(TAG, "gyro aiming: off");
            return null;
        }
        float yaw = scale(in.getFloatExtra(EXTRA_GYRO_YAW_SCALE, SCALE_DEFAULT));
        float pitch = scale(in.getFloatExtra(EXTRA_GYRO_PITCH_SCALE, SCALE_DEFAULT));
        return new GyroAim(activity, yaw, pitch);
    }

    private GyroAim(Activity activity, float yawScale, float pitchScale) {
        this.yawScale = yawScale;
        this.pitchScale = pitchScale;
        display = activity.getWindowManager().getDefaultDisplay();
        sensors = (SensorManager) activity.getSystemService(Activity.SENSOR_SERVICE);
        gyroscope = sensors != null ? sensors.getDefaultSensor(Sensor.TYPE_GYROSCOPE) : null;
        Sensor g = sensors != null ? sensors.getDefaultSensor(Sensor.TYPE_GRAVITY) : null;
        gravity = g != null ? g : (sensors != null ? sensors.getDefaultSensor(Sensor.TYPE_ACCELEROMETER) : null);
        if (gyroscope == null)
            Log.i(TAG, "gyro aiming: on, but the device has no gyroscope");
        else
            Log.i(TAG, String.format(Locale.US, "gyro aiming: on, %.2f horizontal, %.2f vertical (%s; up from %s)",
                    yawScale, pitchScale, gyroscope.getName(), gravity != null ? gravity.getName() : "nothing: 45 degrees back"));
    }

    /** Listens while the game is in front (the activity's onResume). */
    void start() {
        if (listening || gyroscope == null) return;
        lastNanos = 0L;
        sensors.registerListener(this, gyroscope, SensorManager.SENSOR_DELAY_GAME);
        if (gravity != null) sensors.registerListener(this, gravity, SensorManager.SENSOR_DELAY_GAME);
        listening = true;
    }

    /** Stops listening (the activity's onPause). */
    void stop() {
        if (!listening) return;
        sensors.unregisterListener(this);
        listening = false;
    }

    @Override public void onAccuracyChanged(Sensor sensor, int accuracy) {}

    @Override public void onSensorChanged(SensorEvent e) {
        if (e.sensor == gravity) {
            up[0] = e.values[0];
            up[1] = e.values[1];
            up[2] = e.values[2];
            upInScreen = false;
            return;
        }
        if (e.sensor != gyroscope) return;
        long now = e.timestamp;
        float dt = lastNanos != 0L ? (now - lastNanos) * 1e-9f : 0f;
        lastNanos = now;
        if (dt <= 0f) return;

        // The device's axes as the screen's: the display's rotation is the opposite of the device's.
        int rotation = display.getRotation();
        float wx = screenX(e.values[0], e.values[1], rotation), wy = screenY(e.values[0], e.values[1], rotation), wz = e.values[2];
        float ux = upInScreen ? up[0] : screenX(up[0], up[1], rotation);
        float uy = upInScreen ? up[1] : screenY(up[0], up[1], rotation);
        float uz = up[2];
        float upLength = (float) Math.sqrt(ux * ux + uy * uy + uz * uz);
        if (upLength <= 0f) return;

        // Rad/s, counter-clockwise positive: about the world's vertical is a turn to the left, about x a look up.
        float worldYaw = (wy * uy + wz * uz) / upLength;
        float turnAndTwist = (float) Math.hypot(wy, wz);
        float left = Math.signum(worldYaw) * Math.min(Math.abs(worldYaw) * YAW_RELAX, turnAndTwist);
        float yawDegrees = -left * dt * DEGREES_PER_RADIAN * yawScale;
        float pitchDegrees = wx * dt * DEGREES_PER_RADIAN * pitchScale;
        if (yawDegrees == 0f && pitchDegrees == 0f) return;
        try {
            GameActivity.nativeGyroLook(yawDegrees, pitchDegrees);
        } catch (Throwable t) {
            // The engine's library is not loaded: nothing to aim.
            Log.e(TAG, "gyro aiming: no engine to send to", t);
            stop();
        }
    }

    private static float screenX(float x, float y, int rotation) {
        switch (rotation) {
            case Surface.ROTATION_90: return -y;
            case Surface.ROTATION_180: return -x;
            case Surface.ROTATION_270: return y;
            default: return x;
        }
    }

    private static float screenY(float x, float y, int rotation) {
        switch (rotation) {
            case Surface.ROTATION_90: return x;
            case Surface.ROTATION_180: return -y;
            case Surface.ROTATION_270: return -x;
            default: return y;
        }
    }
}
