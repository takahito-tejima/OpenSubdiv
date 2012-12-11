# OpenSubdiv

OpenSubdiv is a set of open source libraries that implement high performance subdivision surface (subdiv) evaluation on massively parallel CPU and GPU architectures. This codepath is optimized for drawing deforming subdivs with static topology at interactive framerates. The resulting limit surface matches Pixar's Renderman to numerical precision.

OpenSubdiv is covered by the [Microsoft Public License](http://www.microsoft.com/en-us/openness/licenses.aspx#MPL), and is free to use for commercial or non-commercial use. This is the same code that Pixar uses internally for animated film production. Our intent is to encourage high performance accurate subdiv drawing by giving away the "good stuff".

The current version is "Release Candidate 1.0" (12/05/2012) and we hope to have "Release 1.0" ready by February 2013. Feel free to use it and let us know what you think.

For more details about OpenSubdiv, see [Pixar Graphics Technologies](http://graphics.pixar.com).


## What's New in Release Candidate 1.0 ?

- [Feature Adaptive GPU Rendering of Catmull-Clark Surfaces](http://research.microsoft.com/en-us/um/people/cloop/tog2012.pdf). 

- New API architecture : we are planning to lock on to this new framework as the basis for backward compatibility, which we will enforce from Release 1.0 onward. Subsequent releases of OpenSubdiv should not break client code.

- DirectX 11 support 


## Quickstart

Basic instructions to get started with the code.

### Dependencies

Cmake will adapt the build based on which dependencies have been successfully discovered and will disable certain features and code examples accordingly.

Please refer to the documentation of each of the dependency packages for specific build and installation instructions.

Required:
* [cmake](http://www.cmake.org/cmake/resources/software.html)
* [GLEW](http://sourceforge.net/projects/glew/) (Windows/Linux only)

Optional:
* [CUDA](http://developer.nvidia.com/category/zone/cuda-zone)
* [OpenCL](http://www.khronos.org/opencl/)
* [GLUT](http://freeglut.sourceforge.net/)
* [Ptex](https://github.com/wdas/ptex)
* [Zlib](http://www.zlib.net) (required for Ptex under Windows)
* [Maya SDK](http://www.autodesk.com/maya/) (sample code for Maya viewport 2.0 primitive)
* [DX11 SDK](http://www.microsoft.com/en-us/download/details.aspx?id=6812)

### Useful cmake options and environment variables

````
-DCMAKE_BUILD_TYPE=[Debug|Release]
-DCUDA_TOOLKIT_ROOT_DIR=[path to CUDA]
-DPTEX_LOCATION=[path to Ptex]
-DGLEW_LOCATION=[path to GLEW]
-DGLUT_LOCATION=[path to GLUT]
-DMAYA_LOCATION=[path to Maya]
````

The paths to Maya, Ptex, GLUT, and GLEW can also be specified through the
following environment variables: `MAYA_LOCATION`, `PTEX_LOCATION`, `GLUT_LOCATION`,
and `GLEW_LOCATION`.


### Build instructions (Linux/OSX/Windows):

__Clone the repository:__

From the GitShell, Cygwin or the CLI :

````
git clone git://github.com/PixarAnimationStudios/OpenSubdiv.git
````

Alternatively, on Windows, GIT also provides a GUI to perform this operation.

__Generate Makefiles:__

Assuming that we want the binaries installed into a "build" directory at the root of the OpenSubdiv tree :
````
cd OpenSubdiv
mkdir build
cd build
````

Here is an example cmake configuration script for a full typical windows-based build that can be run in GitShell :

````
#/bin/tcsh

# Replace the ".." with a full path to the root of the OpenSubdiv source tree if necessary
"c:/Program Files (x86)/CMake 2.8/bin/cmake.exe" \
    -G "Visual Studio 10 Win64" \
    -D "GLEW_LOCATION:string=c:/Program Files/glew-1.9.0" \
    -D "GLUT_LOCATION:string=c:/Program Files/freeglut-2.8.0" \
    -D "OPENCL_INCLUDE_DIRS:string=c:/ProgramData/NVIDIA Corporation/NVIDIA GPU Computing SDK 4.2/OpenCL/common/inc" \
    -D "_OPENCL_CPP_INCLUDE_DIRS:string=c:/ProgramData/NVIDIA Corporation/NVIDIA GPU Computing SDK 4.2/OpenCL/common/inc" \
    -D "OPENCL_LIBRARIES:string=c:/ProgramData/NVIDIA Corporation/NVIDIA GPU Computing SDK 4.2/OpenCL/common/lib/x64/OpenCL.lib" \
    -D "MAYA_LOCATION:string=c:/Program Files/Autodesk/Maya2013.5" \
    -D "PTEX_LOCATION:string=c:/Users/opensubdiv/demo/src/ptex/x64" \
    ..

# copy Ptex dependencies (Windows only)
mkdir -p bin/{Debug,Release}
\cp -f c:/Users/opensubdiv/demo/src/zlib-1.2.7/contrib/vstudio/vc10/x64/ZlibDllRelease/zlibwapi.dll bin/Debug/
\cp -f c:/Users/opensubdiv/demo/src/zlib-1.2.7/contrib/vstudio/vc10/x64/ZlibDllRelease/zlibwapi.dll bin/Release/
\cp -f c:/Users/opensubdiv/demo/src/ptex/x64/lib/Ptex.dll bin/Debug/
\cp -f c:/Users/opensubdiv/demo/src/ptex/x64/lib/Ptex.dll bin/Release/
````

Alternatively, you can use the cmake GUI or run the commands from the CLI.

__Build the project:__

Windows : launch VC++ with the solution generated by cmake in your build directory.

*Nix : run make in your build directory


## Standalone viewers

OpenSubdiv builds a number of standalone viewers that demonstrate various aspects of the software.

__Common Keyboard Shortcuts:__

````
Left mouse button drag   : orbit camera
Middle mouse button drag : pan camera
Right mouse button       : dolly camera
n, p                     : next/prev model
1, 2, 3, 4, 5, 6, 7      : specify adaptive isolation or uniform refinment level
+, -                     : increase / decrease tessellation 
w                        : switch display mode
q                        : quit
````

## Wish List

There are many things we'd love to do to improve support for subdivs but don't have the resources to. In particular, we would welcome contributions for the following items :

  * The maya plugins don't integrate with Maya shading.  That would be cool.
  * John Lasseter loves looking at film assets in progress on an iPad. If anyone were to get this working on iOS he'd be looking at your code, and the apple geeks in all of us would smile.
  * Alembic support would be wonderful, but we don't use Alembic enough internally to do the work.
  * The precomputation step with hbr can be slow. Does anyone have thoughts on higher performance with topology rich data structures needed for feature adaptive subdivision? Maybe a class that packs adjacency into blocks of indices efficiently, or supports multithreading ?
