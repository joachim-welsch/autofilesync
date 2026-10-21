plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.welschsoftware.autofilesync.spike"
    compileSdk = 34
    defaultConfig {
        applicationId = "com.welschsoftware.autofilesync.spike"
        minSdk = 26
        targetSdk = 34
        versionCode = 1
        versionName = "0.0.1-spike"
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions { jvmTarget = "17" }
    packaging { resources { excludes += setOf("META-INF/DEPENDENCIES", "META-INF/LICENSE*", "META-INF/NOTICE*") } }
}

dependencies {
    implementation("org.apache.sshd:sshd-core:2.14.0")
    implementation("org.apache.sshd:sshd-sftp:2.14.0")
    implementation("org.slf4j:slf4j-nop:1.7.36")
}
