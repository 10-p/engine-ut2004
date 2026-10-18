package com.ut2004.android;

import android.os.StatFs;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Locale;

/**
 * Filesystem helpers for the launcher: free-space / dir-size readouts, an incremental
 * external->internal tree sync, and recursive delete. The heavy calls (dirSize / syncTree /
 * deleteTree) are meant to run on a background thread; MainActivity marshals the {@link Listener}
 * callbacks back to the UI thread itself.
 */
final class Storage {
    private Storage() {}

    interface Listener {
        /** Periodic progress. {@code doneBytes}/{@code totalBytes} are src bytes examined so far
         *  (skips included, for a smooth bar); {@code copiedBytes} is what was actually written. */
        void onProgress(long doneBytes, long totalBytes, long copiedBytes, String currentRel);
    }

    /** Free bytes available to the app on the volume that holds {@code path}. */
    static long freeBytes(File path) {
        try {
            return new StatFs(path.getAbsolutePath()).getAvailableBytes();
        } catch (Throwable t) {
            return -1L;
        }
    }

    /** Total bytes of all regular files under {@code dir} (0 if absent). */
    static long dirSize(File dir) {
        if (dir == null || !dir.exists()) return 0L;
        if (dir.isFile()) return dir.length();
        long sum = 0L;
        File[] kids = dir.listFiles();
        if (kids != null) for (File k : kids) sum += dirSize(k);
        return sum;
    }

    /**
     * Incrementally copy the tree at {@code src} into {@code dst}: for each regular file, create the
     * parent dirs and copy ONLY if the destination is missing or a different size -- so re-running
     * just adds new/changed files (e.g. a Maps/ addition) and a copy interrupted by low space simply
     * resumes. Each file is streamed to a {@code .part} sibling then atomically renamed, so a killed
     * copy never leaves a wrong-sized file that the size check would later skip.
     *
     * Returns null on success, or a human-readable message if it stopped early (ENOSPC / IOException)
     * -- a partial copy is expected on space-constrained devices (the full tree is about 11 GB).
     */
    static String syncTree(File src, File dst, Listener cb) {
        if (src == null || !src.isDirectory()) return "Nothing to copy at " + src;
        long total = dirSize(src);
        long[] progress = new long[2]; // {doneBytes, copiedBytes}
        try {
            copyRecursive(src, dst, src, total, progress, cb);
            return null;
        } catch (IOException e) {
            String msg = e.getMessage() == null ? e.toString() : e.getMessage();
            if (msg != null && (msg.contains("ENOSPC") || msg.toLowerCase(Locale.US).contains("no space"))) {
                return "Out of space after copying " + human(progress[1]) + " — internal storage is full. "
                        + "The full game may not fit here; free space or run from External instead.";
            }
            return "Stopped after " + human(progress[1]) + " copied (" + msg + ")";
        }
    }

    private static void copyRecursive(File src, File dst, File root, long total, long[] progress, Listener cb)
            throws IOException {
        File[] kids = src.listFiles();
        if (kids == null) return;
        for (File k : kids) {
            File target = new File(dst, k.getName());
            if (k.isDirectory()) {
                copyRecursive(k, target, root, total, progress, cb);
            } else {
                if (!target.exists() || target.length() != k.length()) {
                    File parent = target.getParentFile();
                    if (parent != null && !parent.exists() && !parent.mkdirs())
                        throw new IOException("cannot create " + parent);
                    copyFile(k, target);
                    progress[1] += k.length();
                }
                progress[0] += k.length();
                if (cb != null) cb.onProgress(progress[0], total, progress[1], rel(root, k));
            }
        }
    }

    private static void copyFile(File src, File dst) throws IOException {
        File tmp = new File(dst.getAbsolutePath() + ".part");
        InputStream in = null;
        OutputStream out = null;
        try {
            in = new FileInputStream(src);
            out = new FileOutputStream(tmp);
            byte[] buf = new byte[1 << 16];
            int n;
            while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
            out.flush();
        } finally {
            if (out != null) try { out.close(); } catch (IOException ignored) {}
            if (in != null) try { in.close(); } catch (IOException ignored) {}
        }
        if (dst.exists() && !dst.delete()) { tmp.delete(); throw new IOException("cannot replace " + dst); }
        if (!tmp.renameTo(dst)) { tmp.delete(); throw new IOException("cannot finalize " + dst); }
    }

    /**
     * Copy a single regular file, overwriting the destination (used to export settings/saves).
     * Returns null both when copied and when {@code src} is absent (not an error); a message on I/O failure.
     */
    static String copyFileInto(File src, File dst) {
        if (src == null || !src.isFile()) return null;
        try {
            File parent = dst.getParentFile();
            if (parent != null && !parent.exists() && !parent.mkdirs())
                throw new IOException("cannot create " + parent);
            copyFile(src, dst);
            return null;
        } catch (IOException e) {
            return e.getMessage() == null ? e.toString() : e.getMessage();
        }
    }

    /** Recursively delete {@code f}. Returns true iff everything was removed. */
    static boolean deleteTree(File f) {
        if (f == null || !f.exists()) return true;
        boolean ok = true;
        if (f.isDirectory()) {
            File[] kids = f.listFiles();
            if (kids != null) for (File k : kids) ok &= deleteTree(k);
        }
        return f.delete() && ok;
    }

    private static String rel(File root, File f) {
        String rp = root.getAbsolutePath(), fp = f.getAbsolutePath();
        return fp.startsWith(rp) && fp.length() > rp.length() ? fp.substring(rp.length() + 1) : f.getName();
    }

    static String human(long bytes) {
        if (bytes < 0) return "?";
        if (bytes < 1024) return bytes + " B";
        double kb = bytes / 1024.0;
        if (kb < 1024) return String.format(Locale.US, "%.0f KB", kb);
        double mb = kb / 1024.0;
        if (mb < 1024) return String.format(Locale.US, "%.1f MB", mb);
        return String.format(Locale.US, "%.2f GB", mb / 1024.0);
    }
}
