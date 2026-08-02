pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}

dependencyResolutionManagement {
    repositoriesMode = RepositoriesMode.FAIL_ON_PROJECT_REPOS
    repositories {
        google()
        mavenCentral()
    }
}

rootProject.name = "music-lyric-player-skia-example"

includeBuild("../../platform/android") {
    dependencySubstitution {
        substitute(module("io.github.music-lyric:player-skia")).using(project(":"))
    }
}
