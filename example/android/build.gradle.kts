plugins {
    id("com.android.application") version "9.3.1"
}

val packageVersion: String = providers.fileContents(layout.projectDirectory.file("../../VERSION.txt")).asText.get().trim()

// The shared libraries are built by CMake rather than by Gradle, and `make package-android` is what stages them here.
val stagedLibraries = layout.projectDirectory.dir("../../platform/android/src/main/jniLibs")

android {
    namespace = "music.lyric.player.skia.example"
    compileSdk = 37
    compileSdkMinor = 1
    buildToolsVersion = "36.0.0"

    defaultConfig {
        applicationId = "music.lyric.player.skia.example"

        // The library refuses to link below this, and an app cannot ask for less than what it links against.
        minSdk = 24
        targetSdk = 37

        versionCode = 1
        versionName = packageVersion
    }

    buildTypes {
        release {
            // The aar carries consumer rules keeping the native entry points and the callback class, and this is what puts them to work.
            isMinifyEnabled = true
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"))
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}

// An app without them builds happily and only fails at the first native call, so this says which command to run instead.
val checkNativeLibraries = tasks.register("checkNativeLibraries") {
    val directory = stagedLibraries.asFile

    doLast {
        val abis = directory.listFiles().orEmpty().filter { it.isDirectory }
        if (abis.isEmpty()) {
            throw GradleException("no native library is staged in $directory, run `make package-android` from the repository root first")
        }

        logger.lifecycle("staged abis: ${abis.joinToString { it.name }}")
    }
}

tasks.named("preBuild") {
    dependsOn(checkNativeLibraries)
}

dependencies {
    // Resolved to the module in this repository by the substitution in settings.gradle.kts, so the version only says which release the demo belongs to.
    implementation("io.github.music-lyric:player-skia:$packageVersion")

    // ComponentActivity and the file picker contracts; everything else the demo draws with comes from android.jar.
    implementation("androidx.activity:activity:1.13.0")
}
