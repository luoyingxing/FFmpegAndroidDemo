# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.4.1)

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

add_library( # Sets the name of the library.
             native-lib

             # Sets the library as a shared library.
             SHARED

             # Provides a relative path to your source file(s).
             src/main/cpp/native-lib.c )

# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

#----------------avcodec-57----------------------
add_library(
            avcodec-57
            STATIC
            IMPORTED
            )
set_target_properties(
    avcodec-57
    PROPERTIES IMPORTED_LOCATION
    # change path when ues other computer
    E:/AndroidStudioProject/FFmpegAndroidDemo/app/src/main/cpp/libs/libavcodec-57.so
    )
include_directories(
    src/main/cpp/include/libavcodec/
)
#--------------------------------------
#----------------avdevice-57----------------------
add_library(
            avdevice-57
            STATIC
            IMPORTED
            )
set_target_properties(
    avdevice-57
    PROPERTIES IMPORTED_LOCATION
    # change path when ues other computer
    E:/AndroidStudioProject/FFmpegAndroidDemo/app/src/main/cpp/libs/libavdevice-57.so
    )
include_directories(
    src/main/cpp/include/libavdevice/
)
#--------------------------------------
#----------------avformat-57----------------------
add_library(
            avformat-57
            STATIC
            IMPORTED
            )
set_target_properties(
    avformat-57
    PROPERTIES IMPORTED_LOCATION
    # change path when ues other computer
    E:/AndroidStudioProject/FFmpegAndroidDemo/app/src/main/cpp/libs/libavformat-57.so
    )
include_directories(
    src/main/cpp/include/libavformat/
)
#--------------------------------------
#----------------avfilter-6----------------------
add_library(
            avfilter-6
            STATIC
            IMPORTED
            )
set_target_properties(
    avfilter-6
    PROPERTIES IMPORTED_LOCATION
    # change path when ues other computer
    E:/AndroidStudioProject/FFmpegAndroidDemo/app/src/main/cpp/libs/libavfilter-6.so
    )
include_directories(
    src/main/cpp/include/libavfilter/
)
#--------------------------------------
#----------------avutil-55----------------------
add_library(
            avutil-55
            STATIC
            IMPORTED
            )
set_target_properties(
    avutil-55
    PROPERTIES IMPORTED_LOCATION
    # change path when ues other computer
    E:/AndroidStudioProject/FFmpegAndroidDemo/app/src/main/cpp/libs/libavutil-55.so
    )
include_directories(
    src/main/cpp/include/libavutil/
)
#--------------------------------------
#----------------postproc-54----------------------
add_library(
            postproc-54
            STATIC
            IMPORTED
            )
set_target_properties(
    postproc-54
    PROPERTIES IMPORTED_LOCATION
    # change path when ues other computer
    E:/AndroidStudioProject/FFmpegAndroidDemo/app/src/main/cpp/libs/libpostproc-54.so
    )
include_directories(
    src/main/cpp/include/libpostproc/
)
#--------------------------------------
#----------------swresample-2----------------------
add_library(
            swresample-2
            STATIC
            IMPORTED
            )
set_target_properties(
    swresample-2
    PROPERTIES IMPORTED_LOCATION
    # change path when ues other computer
    E:/AndroidStudioProject/FFmpegAndroidDemo/app/src/main/cpp/libs/libswresample-2.so
    )
include_directories(
    src/main/cpp/include/libswresample/
)
#--------------------------------------
#----------------swscale-4----------------------
add_library(
            swscale-4
            STATIC
            IMPORTED
            )
set_target_properties(
    swscale-4
    PROPERTIES IMPORTED_LOCATION
    # change path when ues other computer
    E:/AndroidStudioProject/FFmpegAndroidDemo/app/src/main/cpp/libs/libswscale-4.so
    )
include_directories(
    src/main/cpp/include/libswscale/
)
#--------------------------------------

find_library( # Sets the name of the path variable.
              log-lib

              # Specifies the name of the NDK library that
              # you want CMake to locate.
              log )

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( # Specifies the target library.
                       native-lib
                       avcodec-57
                       avdevice-57
                       avformat-57
                       avfilter-6
                       avutil-55
                       postproc-54
                       swresample-2
                       swscale-4

                       # Links the target library to the log library
                       # included in the NDK.
                       ${log-lib} )