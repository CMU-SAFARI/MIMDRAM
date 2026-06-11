FROM ubuntu:14.04

ENV DEBIAN_FRONTEND=noninteractive

# Install gem5 dependencies
RUN apt-get update && apt-get install -y \
    build-essential m4 scons zlib1g-dev libprotobuf-dev \
    protobuf-compiler libgoogle-perftools-dev python-dev swig \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /

# Copy local files
COPY gem5 gem5/
COPY microworkloads microworkloads/
COPY llvm-pim-pass llvm-pim-pass/

# Apply protobuf fix
RUN sed -i "555i\    main.Append(CCFLAGS=['-DPROTOBUF_INLINE_NOT_IN_HEADERS=0'])" gem5/SConstruct

# Build gem5
RUN cd gem5 && scons build/X86/gem5.opt -j$(nproc)

# Compile microworkloads
RUN cd microworkloads && chmod +x compile.sh && ./compile.sh

CMD ["/bin/bash"]