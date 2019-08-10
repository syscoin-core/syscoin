## ci scripts

This directory contains scripts for each build step in each build stage.

Currently three stages `lint`, `extended_lint` and `test` are defined. Each stage has its own lifecycle, similar to the
[Travis CI lifecycle](https://docs.travis-ci.com/user/job-lifecycle#the-job-lifecycle). Every script in here is named
and numbered according to which stage and lifecycle step it belongs to.

### Running a stage locally

To allow for a wide range of tested environments, but also ensure reproducibility to some extent, the test stage
requires `docker` to be installed. To install all requirements on Ubuntu, run

```
sudo apt install docker.io ccache bash git
```

To run the test stage,

```
./ci/test_run_all.sh
```

Be aware that the tests will be build and run in-place, so please run at your own risk.