
package com.dishii.soh;
import org.libsdl.app.SDLActivity;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.FileOutputStream;
import android.util.Log;

//This class is the main SDLActivity and just sets up a bunch of default files
public class MainActivity extends SDLActivity{

    private static final String TAG = "SoH";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // The game looks for soh.otr in the app's own external folder as soon as SDL starts its native thread,
        // which happens after super.onCreate() has returned. So the bundled files are put there first.
        // getExternalFilesDir() needs no storage permission, so nothing here waits for a permission prompt
        // (a denied prompt used to leave the game without soh.otr: "soh.otr file is missing").
        doVersionCheck();
        setupFiles();

        super.onCreate(savedInstanceState);
    }

    private void doVersionCheck(){
        SharedPreferences preferences = getSharedPreferences("com.dishii.soh.prefs",Context.MODE_PRIVATE);
        int currentVersion = BuildConfig.VERSION_CODE; // Use your app's version code
        int storedVersion = preferences.getInt("appVersion", 1);

        if (currentVersion > storedVersion) {
            deleteOutdatedAssets();
            preferences.edit().putInt("appVersion", currentVersion).apply();
        }
    }

    private void deleteOutdatedAssets(){
        File externalSohFile = new File(getExternalFilesDir(null), "soh.otr");
        externalSohFile.delete();
        File externalOotFile = new File(getExternalFilesDir(null), "oot.otr");
        externalOotFile.delete();
        File externalOotMqFile = new File(getExternalFilesDir(null), "oot-mq.otr");
        externalOotMqFile.delete();
        File externalAssetsFolder = new File(getExternalFilesDir(null), "assets");
        deleteRecursive(externalAssetsFolder);

    }

    private void deleteRecursive(File fileOrDirectory) {
        if (fileOrDirectory.isDirectory()) {
            for (File child : fileOrDirectory.listFiles()) {
                deleteRecursive(child);
            }
        }
        fileOrDirectory.delete();
    }



    private void setupFiles(){
        File filesDir = getExternalFilesDir(null);
        if (filesDir == null) {
            Log.e(TAG, "No external files folder is available");
            return;
        }

        //Copy assets folder for rom extraction. The marker is written only when the copy has finished, so a copy
        //that was cut short is continued on the next start instead of being taken for complete.
        File externalAssetsDir = new File(filesDir, "assets");
        File assetsCopied = new File(externalAssetsDir, ".copied");
        if (!assetsCopied.exists()) {
            try {
                externalAssetsDir.mkdirs();
                AssetCopyUtil.copyAssetsToExternal(this, "assets", externalAssetsDir.getAbsolutePath());
                assetsCopied.createNewFile();
            } catch (IOException e) {
                Log.e(TAG, "Copying the bundled assets failed", e);
            }
        }

        //Create empty mods folder
        new File(filesDir, "mods").mkdirs();

        //Copy soh.otr
        File externalSohOtrFile = new File(filesDir, "soh.otr");
        if (!externalSohOtrFile.exists()) {
            try {
                AssetCopyUtil.copyAssetFile(this, "soh.otr", externalSohOtrFile);
            } catch (IOException e) {
                Log.e(TAG, "Copying soh.otr failed", e);
            }
        }
    }

    private native void nativeHandleSelectedFile(String filePath);

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == 0 && resultCode == RESULT_OK) {
            Uri selectedFileUri = data.getData();
            String fileName = "ZELOOTD.z64";
            File destinationDirectory = getExternalFilesDir(null); // The second argument can specify a subdirectory, or you can pass null to use the root directory.
            File destinationFile = new File(destinationDirectory, fileName);

            if (destinationDirectory != null) {
                try {
                    InputStream in = getContentResolver().openInputStream(selectedFileUri);
                    OutputStream out = new FileOutputStream(destinationFile);

                    byte[] buffer = new byte[4096];
                    int bytesRead;
                    while ((bytesRead = in.read(buffer)) != -1) {
                        out.write(buffer, 0, bytesRead);
                    }

                    in.close();
                    out.close();
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
            nativeHandleSelectedFile(destinationFile.getPath());
        }
    }

    public void openFilePicker() {
        // Create an Intent to open the file picker dialog
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.setType("*/*");

        // Start the file picker dialog
        startActivityForResult(intent, 0);
    }

    // Check if external storage is available and writable
    private boolean isExternalStorageWritable() {
        String state = Environment.getExternalStorageState();
        return Environment.MEDIA_MOUNTED.equals(state);
    }

}
