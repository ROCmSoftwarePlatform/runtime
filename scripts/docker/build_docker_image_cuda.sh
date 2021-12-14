#!/bin/bash

# exit immediately on failure, or if an undefined variable is used
set -eux

BASE_IMAGE="nvidia/cuda:11.1-cudnn8-devel-ubuntu18.04"

DOCKER_IMAGE="rocm/tensorflow-private:ubuntu18.04-cuda11.1-cudnn8-tfrt"

DOCKER_BUILD_ARGS=" \
  --build-arg BASE_IMAGE=$BASE_IMAGE \
  --build-arg ROCM_OR_CUDA=CUDA \
"

DOCKERFILE=Dockerfile.ubuntu18.04-rocm-tfrt

docker build -t $DOCKER_IMAGE -f $DOCKERFILE $DOCKER_BUILD_ARGS .
