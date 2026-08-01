plugins {
    id("com.android.library") version "9.3.1"
    id("com.vanniktech.maven.publish") version "0.37.0"
}

val packageVersion: String = providers.gradleProperty("VERSION_NAME").get()

android {
    namespace = "music.lyric.player.skia"
    compileSdk = 37
    compileSdkMinor = 1
    ndkVersion = "29.0.14206865"
    buildToolsVersion = "36.0.0"

    defaultConfig {
        // Vulkan reached the platform in Android 7.0, and the GPU backend refuses to link below that.
        minSdk = 24

        // AGP 9 otherwise holds consumers to this module's own compileSdk, which is far more than a library carrying no platform resources needs.
        aarMetadata { minCompileSdk = 34 }

        consumerProguardFiles("consumer-rules.pro")
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}

// The annotations are compile time only, so they never reach the consumer's dependency graph.
// Everything else this module touches lives in android.jar.
dependencies {
    compileOnly("androidx.annotation:annotation:1.10.0")
}

mavenPublishing {
    coordinates("io.github.music-lyric", "player-skia", packageVersion)
    publishToMavenCentral()
    signAllPublications()

    pom {
        name.set("Music Lyric Player Skia")
        description.set("Skia lyric player for Android.")
        url.set("https://github.com/music-lyric/music-lyric-player-skia")

        licenses {
            license {
                name.set("MIT License")
                url.set("https://github.com/music-lyric/music-lyric-player-skia/blob/main/LICENSE")
            }
        }

        developers {
            developer {
                id.set("folltoshe")
                name.set("folltoshe")
                url.set("https://github.com/folltoshe")
            }
        }

        scm {
            url.set("https://github.com/music-lyric/music-lyric-player-skia")
            connection.set("scm:git:git://github.com/music-lyric/music-lyric-player-skia.git")
            developerConnection.set("scm:git:ssh://git@github.com/music-lyric/music-lyric-player-skia.git")
        }
    }
}
