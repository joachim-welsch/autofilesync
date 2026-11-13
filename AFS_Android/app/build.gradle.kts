import java.util.Properties
import org.jetbrains.kotlin.gradle.dsl.JvmTarget

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.plugin.compose")
}

android {
    namespace = "com.welschsoftware.autofilesync"
    compileSdk = 34
    defaultConfig {
        applicationId = "com.welschsoftware.autofilesync"
        minSdk = 26
        targetSdk = 34
        versionCode = 1
        versionName = "0.1.0"
    }
    val keystoreFile = rootProject.file("keystore.properties")  // never committed; tools/make-release-key.sh writes it
    signingConfigs {
        if (keystoreFile.exists()) {
            val keystore = Properties().apply { keystoreFile.inputStream().use(::load) }
            create("release") {
                storeFile = file(keystore.getProperty("storeFile"))
                storePassword = keystore.getProperty("storePassword")
                keyAlias = keystore.getProperty("keyAlias")
                keyPassword = keystore.getProperty("keyPassword")
            }
        }
    }
    buildTypes {
        debug {
            signingConfig = signingConfigs.findByName("release") ?: signingConfigs.getByName("debug")  // same key lets debug and release builds replace each other on the phone
        }
        release {
            isMinifyEnabled = false
            signingConfig = signingConfigs.findByName("release")  // unsigned without keystore.properties, as F-Droid builds it
        }
    }
    dependenciesInfo {
        includeInApk = false  // F-Droid rejects the Google-encrypted dependency block
        includeInBundle = false
    }
    buildFeatures { compose = true }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    packaging { resources { excludes += setOf("META-INF/DEPENDENCIES", "META-INF/LICENSE*", "META-INF/NOTICE*", "META-INF/INDEX.LIST") } }
}

kotlin { compilerOptions { jvmTarget = JvmTarget.JVM_17 } }

dependencies {
    val composeBom = platform("androidx.compose:compose-bom:2024.09.03")
    implementation(composeBom)
    implementation("androidx.compose.material3:material3")
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.ui:ui-tooling-preview")
    implementation("androidx.core:core-ktx:1.13.1")
    implementation("androidx.activity:activity-compose:1.9.2")
    implementation("androidx.lifecycle:lifecycle-viewmodel-compose:2.8.6")
    implementation("androidx.lifecycle:lifecycle-runtime-compose:2.8.6")
    implementation("androidx.camera:camera-core:1.3.4")
    implementation("androidx.camera:camera-camera2:1.3.4")
    implementation("androidx.camera:camera-lifecycle:1.3.4")
    implementation("androidx.camera:camera-view:1.3.4")
    implementation("com.google.zxing:core:3.5.3")
    implementation("org.apache.sshd:sshd-core:2.14.0")
    implementation("org.apache.sshd:sshd-sftp:2.14.0")
    implementation("org.slf4j:slf4j-nop:1.7.36")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.8.1")
    implementation("androidx.work:work-runtime-ktx:2.9.1")
}
