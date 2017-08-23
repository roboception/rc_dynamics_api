# Roboception Dynamics API

The rc_dynamics_api provides an API for easy handling of the dynamic-state data 
streams provided by Roboception's [rc_visard](http://roboception.com/en/rc_visard-en/) 
stereo camera with self-localization.

Dynamic-state estimates of the rc_visard relate to its self-localization and 
ego-motion estimation. These states refer to rc_visard's current pose, velocity, 
or acceleration and are published on demand via several data streams. For a 
complete list and descriptions of these dynamics states and the respective data
streams please refer to rc_visard's user manual.


## Dependencies

- **[C++ Requests](https://github.com/whoshuu/cpr) (version 1.3.0):** 
Requesting and deleting data streams is done via rc_visard's REST-API. This 
library provides an easy-to-use interface for doing REST-API calls. 
- **[JSON for Modern C++](https://github.com/nlohmann/json) (version v2.0.0):**
A simple and modern C++ JSON parsing library.   
- **[Google Protocol Buffers:](https://developers.google.com/protocol-buffers/)**
The data sent via rc_visard's data streams is serialized via Google protocol 
message definitions (/roboception/msgs). After receiving the data, the 
rc_dynamics_api needs these definitions in order to de-serialized it. This 
project requires both the `protobuf-compiler` for compiling the protocol buffer 
definition files and the `libprotobuf` C++ library.

## Build/Installation

Some libraries listed above are included as git submodules in this repository. 
Hence, before building this lib you need to
    
    git submodule update --init --recursive
    
Then building follows the typical cmake buid-flow

    mkdir build && cd build
    cmake ..
    make
    
## Tool

TODO


## Links

- http://www.roboception.com