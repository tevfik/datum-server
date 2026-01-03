allprojects {
    repositories {
        google()
        mavenCentral()
    }
}

val newBuildDir: Directory =
    rootProject.layout.buildDirectory
        .dir("../../build")
        .get()
rootProject.layout.buildDirectory.value(newBuildDir)

subprojects {
    val newSubprojectBuildDir: Directory = newBuildDir.dir(project.name)
    project.layout.buildDirectory.value(newSubprojectBuildDir)
}
subprojects {
    project.evaluationDependsOn(":app")
}

subprojects {
    afterEvaluate {
        val android = extensions.findByName("android")
        if (android != null) {
            try {
                // Use reflection to check/set namespace to avoid compilation errors 
                // if AGP classes aren't in the root script classpath
                val getNamespace = android.javaClass.getMethod("getNamespace")
                val namespace = getNamespace.invoke(android)
                
                if (namespace == null) {
                    val setNamespace = android.javaClass.getMethod("setNamespace", String::class.java)
                    val newNamespace = "com.example.${project.name.replace("-", "_")}"
                    setNamespace.invoke(android, newNamespace)
                    println("Added namespace '$newNamespace' to project '${project.name}'")
                }
            } catch (e: Exception) {
                // Ignore reflection errors or mismatched AGP versions
                println("Failed to set namespace for ${project.name}: $e")
            }
        }
    }
}

tasks.register<Delete>("clean") {
    delete(rootProject.layout.buildDirectory)
}
