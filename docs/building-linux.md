## Build instructions for Linux using Docker

### Obtain your API credentials

You will require **api_id** and **api_hash** to access the Telegram API servers. To learn how to obtain them [click here][api_credentials].

### Clone source code

    git clone --recursive https://github.com/telegramdesktop/tdesktop.git

### Prepare libraries

Go to the `tdesktop` directory and run

    docker build -t tdesktop:centos_env Telegram/build/docker/centos_env/

### Building the project

##### Make sure:
   - You're still in the `tdesktop` directory
   - You have atleast 8GB Ram and 10GB Swap
   - You have `$API_ID` and `$API_HASH` in your environmental variables

#### Release Build

    docker run --rm -it \                 
        --cpus=$(($(nproc) - 1)) \
        -v $PWD:/usr/src/tdesktop \
        tdesktop:centos_env \
        /usr/src/tdesktop/Telegram/build/docker/centos_env/build.sh \
        -D TDESKTOP_API_ID=$API_ID \
        -D TDESKTOP_API_HASH=$API_HASH \
        -D DESKTOP_APP_USE_PACKAGED=OFF \
        -D DESKTOP_APP_DISABLE_CRASH_REPORTS=OFF

#### Debug Build

    docker run --rm -it \                 
        --cpus=$(($(nproc) - 1)) \
        -v $PWD:/usr/src/tdesktop \
        -e DEBUG=1 \
        tdesktop:centos_env \
        /usr/src/tdesktop/Telegram/build/docker/centos_env/build.sh \
        -D TDESKTOP_API_ID=$API_ID \
        -D TDESKTOP_API_HASH=$API_HASH \
        -D DESKTOP_APP_USE_PACKAGED=OFF \
        -D DESKTOP_APP_DISABLE_CRASH_REPORTS=OFF

The built files will be in the `out` directory.

[api_credentials]: api_credentials.md
