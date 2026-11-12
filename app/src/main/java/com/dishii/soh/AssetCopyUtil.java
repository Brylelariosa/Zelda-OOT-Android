package com.dishii.soh;

import android.content.Context;
import android.content.res.AssetManager;
import java.io.Closeable;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class AssetCopyUtil {

    private static final int BUFFER_SIZE = 64 * 1024;

    /**
     * Copies a folder of the APK's assets to external storage. Files that already exist are left alone, so a copy
     * that was interrupted can simply be run again.
     */
    public static void copyAssetsToExternal(Context context, String assetsFolderPath, String externalFolderPath) throws IOException {
        copyFolder(context.getAssets(), assetsFolderPath, externalFolderPath, new byte[BUFFER_SIZE]);
    }

    /** Copies one asset file to the given file and replaces it if it exists. */
    public static void copyAssetFile(Context context, String assetPath, File target) throws IOException {
        copyFile(context.getAssets(), assetPath, target, new byte[BUFFER_SIZE]);
    }

    private static void copyFolder(AssetManager assetManager, String assetsFolderPath, String externalFolderPath, byte[] buffer) throws IOException {
        String[] assetFiles = assetManager.list(assetsFolderPath);
        if (assetFiles == null) {
            return;
        }

        for (String assetFile : assetFiles) {
            String assetPath = assetsFolderPath + File.separator + assetFile;
            String externalPath = externalFolderPath + File.separator + assetFile;

            String[] children = assetManager.list(assetPath);
            if (children != null && children.length > 0) {
                // It's a directory: create it and copy its contents
                new File(externalPath).mkdirs();
                copyFolder(assetManager, assetPath, externalPath, buffer);
            } else {
                File externalFile = new File(externalPath);
                if (!externalFile.exists()) {
                    copyFile(assetManager, assetPath, externalFile, buffer);
                }
            }
        }
    }

    // The data is written under a temporary name and moved into place at the end, so a copy that is cut short
    // (app closed, storage full) never leaves a half written file that later looks complete.
    private static void copyFile(AssetManager assetManager, String assetPath, File target, byte[] buffer) throws IOException {
        File temp = new File(target.getPath() + ".tmp");
        InputStream in = null;
        OutputStream out = null;
        try {
            in = assetManager.open(assetPath);
            out = new FileOutputStream(temp);

            int read;
            while ((read = in.read(buffer)) != -1) {
                out.write(buffer, 0, read);
            }
            out.close();
            out = null;

            if (!temp.renameTo(target)) {
                throw new IOException("Could not move " + temp + " to " + target);
            }
        } finally {
            closeQuietly(in);
            closeQuietly(out);
            temp.delete(); // nothing left to delete after a successful move
        }
    }

    private static void closeQuietly(Closeable closeable) {
        if (closeable != null) {
            try {
                closeable.close();
            } catch (IOException ignored) {
            }
        }
    }
}
