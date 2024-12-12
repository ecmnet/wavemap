^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Changelog for package wavemap_rviz_plugin
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

2.2.1 (2024-12-10)
------------------

2.2.0 (2024-11-25)
------------------

2.1.2 (2024-11-20)
------------------
* Report CPU, wall time and RAM usage when rosbag_processor completes
* Contributors: Victor Reijgwart

2.1.1 (2024-10-24)
------------------
* Address warnings from new cpplint version (v2.0)
* Contributors: Victor Reijgwart

2.1.0 (2024-09-16)
------------------

2.0.1 (2024-08-30)
------------------

2.0.0 (2024-08-12)
------------------
* New features

  * Extend Rviz plugin to support rendering ESDFs

* Improvements

  * Use new features of wavemap's C++ library

    * Use classified map to speed up voxel visibility culling

  * Tidy up CMake files

    * Switch from catkin_simple to vanilla catkin
    * Remove dependencies on catkinized gflags, glog and Eigen

* Contributors: Victor Reijgwart

1.6.3 (2023-12-21)
------------------
* Fix a bug in LoD level selection when using Rviz's TopDownOrtho ViewController
* Contributors: Victor Reijgwart

1.6.2 (2023-12-11)
------------------
* Update included Ogre and Rviz headers to support Ubuntu 23
* Include <optional> for std::optional
* Contributors: Lucas Walter

1.6.1 (2023-11-20)
------------------

1.6.0 (2023-10-17)
------------------
* New features

  * Add service and button to reset the wavemap_server's map
  * Add option to load maps directly from disk
  * Add option to only draw surface voxels

* Improvements

  * General Rviz plugin UI improvements
  * Improve Rviz plugin block drawing scheduling
  * Update incremental transmission and Rviz to remove deleted blocks
  * Rename "grid" to "voxels" in Rviz plugin UI and code for clarity
  * Clean up and optimize visibility query handling
  * Clean up and optimize alpha handling
  * Add Tracy annotations for Rviz plugin
  * Refactor wavemap utils

* Bug fixes

  * Fix bug causing delays when drawing blocks with identical timestamps
  * Fix bug causing segfaults upon Rviz plugin instance destruction

* Contributors: Victor Reijgwart

1.5.3 (2023-09-28)
------------------

1.5.2 (2023-09-19)
------------------

1.5.1 (2023-09-08)
------------------

1.5.0 (2023-09-05)
------------------

1.4.0 (2023-08-30)
------------------

1.3.2 (2023-08-28)
------------------

1.3.1 (2023-08-22)
------------------
* Release the code under the BSD-3 license

1.3.0 (2023-08-17)
------------------
* Major refactoring of Rviz plugin architecture and UI

  * Support different render modes (slice; grid) in a single WavemapDisplay instance

    * Each render mode can be configured through its own foldout menu
    * The map is only stored once per plugin instance and shared among the render mode handlers

  * Grid render mode

    * Expose grid color options in the UI
    * Add option to set maximum grid drawing resolution (e.g. to improve frame rate when displaying large maps on computers with no GPU)

  * Improve default settings s.t. it can be used with minimal tuning in most scenarios
  * Remove the option to render map meshes

    * This render mode does not yet produce good iso-surfaces and is currently very slow. It will be reintroduced once its more mature.

* Improve Rviz plugin performance

  * Only redraw map blocks that changed
  * Render grid blocks with Level of Detail based on their distance to the camera
  * Use a work queue and limit the update time per frame, to avoid stalling Rviz when large map changes occur
  * Interface directly with Ogre, instead of using rviz::Pointcloud as an intermediary for rendering

* General

  * Update map <-> ROS msg conversion methods to be consistent with map <-> byte stream conversions
  * Incremental map transmission
    Only publish changed map blocks and add option to control the max message size. This improves transmission stability over unreliable networks and resolves the issue of roscpp dropping messages >1GB.
  * Standardize time definitions

* Contributors: Victor Reijgwart

1.2.0 (2023-08-11)
------------------

1.1.0 (2023-08-09)
------------------

1.0.0 (2023-08-08)
------------------
* First public release
* Contributors: Victor Reijgwart
