PaddlePaddle in Docker Containers
=================================

PaddlePaddle has been working towards easy deployment on both personal
computers and on the cloud and on-premises clusters.  So we chose
Docker image to be our primary form of production distribution.  If
you want to run a PaddlePaddle program `a.py`, you don't need to
install PaddlePaddle, but can just run

.. code-block:: bash

   docker run -d -v $PWD:/work paddlepaddle/paddle /work/a.py

In addition to the production distribution, the deveolopment
environment is also in a Docker image.  So developers don't need to
install tools and configure their development environment.  Instead,
they could just clone the source code, build the development
environmet into a Docker image:

.. code-block:: bash

   git clone https://github.com/PaddlePaddle/Paddle paddle
   cd paddle
   docker build -t paddle:dev .

Now we can build PaddlePaddle:

.. code-block:: bash

   docker run -it -v $PWD:/paddle paddle:dev

This builds PaddlePaddle into :code:`/paddle/build`, or eqivalently,
:code:`paddle/build` on the host.


Install Docker
--------------

If you are using Windows, please follow `this installation guide
<https://www.docker.com/docker-windows>`_.  If you are running Mac,
please follow `this guide <https://www.docker.com/docker-mac>`_. For
various Linux distros, please refer to `this guide
<https://www.docker.com>`_.

You might want to change `Dockers settings
<https://github.com/PaddlePaddle/Paddle/issues/627>`_ to make full use
of your hardware resource on Mac OS X and Windows.


The Development Image
---------------------

The development image should include the following building tools:

   - gcc/clang
   - nvcc
   - Python
   - sphinx
   - woboq
   - sshd

Once we run

.. code-block:: bash

   $ /paddle/paddle/scripts/docker/build.sh

in the development container, we will see .deb packages generated in
`paddle/build` on the host.  If the production environment doesn't
have Docker, we can install the generated .deb packages.

Also, we will see `paddle/build/Dockerfile`, which can be used to
build the produciton image:


1. `paddle:<version>`

   This is the production image, generated using the development image. This image might have multiple variants:

   - GPU/AVX   `paddle:<version>-gpu`
   - GPU/no-AVX  `paddle:<version>-gpu-noavx`
   - no-GPU/AVX  `paddle:<version>`
   - no-GPU/no-AVX  `paddle:<version>-noavx`

   We allow users to choose between GPU and no-GPU because the GPU version image is much larger than then the no-GPU version.

   We allow users the choice between AVX and no-AVX, because some cloud providers don't provide AVX-enabled VMs.


## Development Environment

Here we describe how to use above two images.  We start from considering our daily development environment.

Developers work on a computer, which is usually a laptop or desktop:

<img src="doc/paddle-development-environment.png" width=600 />

or, they might rely on a more sophisticated box (like with GPUs):

<img src="doc/paddle-development-environment-gpu.png" width=600 />

A principle here is that source code lies on the development computer (host) so that editors like Eclipse can parse the source code to support auto-completion.


## Usages

### Build the Development Docker Image

The following commands check out the source code to the host and build the development image `paddle:dev`:

```bash
git clone https://github.com/PaddlePaddle/Paddle paddle
cd paddle
docker build -t paddle:dev .
```

The `docker build` command assumes that `Dockerfile` is in the root source tree.  Note that in this design, this `Dockerfile` is this only one in our repo.


### Build PaddlePaddle from Source Code

Given the development image `paddle:dev`, the following command builds PaddlePaddle from the source tree on the development computer (host):

```bash
docker run -v $PWD:/paddle -e "GPU=OFF" -e "AVX=ON" -e "TEST=ON" paddle:dev
```

This command mounts the source directory on the host into `/paddle` in the container, so the default entry point of `paddle:dev`, `build.sh`, could build the source code with possible local changes.  When it writes to `/paddle/build` in the container, it writes to `$PWD/build` on the host indeed.

`build.sh` builds the following:

- PaddlePaddle binaries,
- `$PWD/build/paddle-<version>.deb` for production installation, and
- `$PWD/build/Dockerfile`, which builds the production Docker image.


### Build the Production Docker Image

The following command builds the production image:

```bash
docker build -t paddle -f build/Dockerfile .
```

This production image is minimal -- it includes binary `paddle`, the shared library `libpaddle.so`, and Python runtime.

### Run PaddlePaddle Applications

Again the development happens on the host.  Suppose that we have a simple application program in `a.py`, we can test and run it using the production image:

```bash
docker run -it -v $PWD:/work paddle /work/a.py
```

But this works only if all dependencies of `a.py` are in the production image. If this is not the case, we need to build a new Docker image from the production image and with more dependencies installs.

### Build and Run PaddlePaddle Applications

We need a Dockerfile in https://github.com/paddlepaddle/book that builds Docker image `paddlepaddle/book:<version>`, basing on the PaddlePaddle production image:

```
FROM paddlepaddle/paddle:<version>
RUN pip install -U matplotlib jupyter ...
COPY . /book
EXPOSE 8080
CMD ["jupyter"]
```

The book image is an example of PaddlePaddle application image.  We can build it

```bash
git clone https://github.com/paddlepaddle/book
cd book
.tools/build_docker.sh
```

### Build and Run Distributed Applications

In our [API design doc](https://github.com/PaddlePaddle/Paddle/blob/develop/doc/design/api.md#distributed-training), we proposed an API that starts a distributed training job on a cluster.  This API need to build a PaddlePaddle application into a Docker image as above and calls kubectl to run it on the cluster.  This API might need to generate a Dockerfile look like above and call `docker build`.

Of course, we can manually build an application image and launch the job using the kubectl tool:

```bash
docker build -f some/Dockerfile -t myapp .
docker tag myapp me/myapp
docker push
kubectl ...
```
