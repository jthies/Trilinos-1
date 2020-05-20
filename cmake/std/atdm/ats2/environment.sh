################################################################################
#
# Set up env on ats2 for ATMD builds of Trilinos
#
# This source script gets the settings from the ATDM_CONFIG_BUILD_NAME var.
#
################################################################################

# ats2 jobs all use the same environmnet changes to the
# sourced script below will impact jobs on both of those
# machines. please be mindful of this when making changes

#
# Handle KOKKOS_ARCH
#

if [[ "$ATDM_CONFIG_COMPILER" == "GNU"* || \
  "$ATDM_CONFIG_COMPILER" == "XL"* ]]; then
  if [[ "$ATDM_CONFIG_KOKKOS_ARCH" == "DEFAULT" || \
    "$ATDM_CONFIG_KOKKOS_ARCH" == "Power9" ]] ; then
    export ATDM_CONFIG_KOKKOS_ARCH=Power9
  else
    echo
    echo "***"
    echo "*** ERROR: KOKKOS_ARCH=$ATDM_CONFIG_KOKKOS_ARCH is not a valid option"
    echo "*** for the compiler GNU.  Replace '$ATDM_CONFIG_KOKKOS_ARCH' in the"
    echo "*** job name with 'Power9'"
    echo "***"
    return
  fi
elif [[ "$ATDM_CONFIG_COMPILER" == "CUDA"* ]] ; then
  if [[ "$ATDM_CONFIG_KOKKOS_ARCH" == "DEFAULT" || \
    "$ATDM_CONFIG_KOKKOS_ARCH" == "Power9" || \
    "$ATDM_CONFIG_KOKKOS_ARCH" == "Volta70" ]] ; then
    export ATDM_CONFIG_KOKKOS_ARCH=Power9,Volta70
  else
    echo
    echo "***"
    echo "*** ERROR: KOKKOS_ARCH=$ATDM_CONFIG_KOKKOS_ARCH is not a valid option"
    echo "*** for the CUDA compiler.  Replace '$ATDM_CONFIG_KOKKOS_ARCH' in the"
    echo "*** job name with one of the following options:"
    echo "***"
    echo "***   'Volta70' Power9 with Volta V-100 GPU (Default)"
    echo "***"
    return
  fi
else
  echo
  echo "***"
  echo "*** ERROR: COMPILER=$ATDM_CONFIG_COMPILER is not supported on this system!"
  echo "***"
  return
fi

echo "Using $ATDM_CONFIG_SYSTEM_NAME compiler stack $ATDM_CONFIG_COMPILER to build $ATDM_CONFIG_BUILD_TYPE code with Kokkos node type $ATDM_CONFIG_NODE_TYPE"

# Some basic settings
export ATDM_CONFIG_ENABLE_SPARC_SETTINGS=ON
export ATDM_CONFIG_USE_NINJA=ON
export ATDM_CONFIG_BUILD_COUNT=8 # Assume building on the shared login node!

# Set ctest -j parallel level for non-CUDA builds
if [ "$ATDM_CONFIG_NODE_TYPE" == "OPENMP" ] ; then
  export ATDM_CONFIG_CTEST_PARALLEL_LEVEL=8
  export OMP_NUM_THREADS=2
else
  export ATDM_CONFIG_CTEST_PARALLEL_LEVEL=16
fi

# NOTE: We do *NOT* purge the modules first like on other systems because that
# messes up the ATS-2 env.  Therefore, it is recommended that the user load a
# new shell and then load one of these envs.

# Load common modules for all builds
module load git/2.20.0

#
# Load compiler modules, TPL modules, and point to SPARC TPL install base dirs
#

if [[ "$ATDM_CONFIG_COMPILER" == *"GNU-7.3.1_SPMPI-rolling" ]]; then
  module load sparc-dev/gcc-7.3.1_spmpi-rolling

  export COMPILER_ROOT=/usr/tce/packages/gcc/gcc-7.3.1
  export LD_LIBRARY_PATH=${COMPILER_ROOT}/lib:${LD_LIBRARY_PATH}
  export BINUTILS_ROOT=${COMPILER_ROOT}
  export LIBRARY_PATH=${BINUTILS_ROOT}/lib
  export LIBRARY_PATH=${CBLAS_ROOT}/lib:${LIBRARY_PATH}
  export INCLUDE=${BINUTILS_ROOT}/include:${INCLUDE}
  export CPATH=${BINUTILS_ROOT}/include:${CPATH}

elif [[ "$ATDM_CONFIG_COMPILER" == *"XL-2019.08.20_SPMPI-rolling_DISABLED" ]]; then
  module load xl/2019.08.20
  module load lapack/3.8.0-xl-2019.08.20
  module load gmake/4.2.1

  # Ninja not available for XL until cmake 3.17.0
  export ATDM_CONFIG_USE_NINJA=OFF

  export CBLAS_ROOT=/usr/tcetmp/packages/lapack/lapack-3.8.0-P9-xl-2019.08.20
  export LAPACK_ROOT=/usr/tcetmp/packages/lapack/lapack-3.8.0-P9-xl-2019.08.20
  export COMPILER_ROOT=/usr/tce/packages/xl/xl-2019.08.20
  export SPARC_HDF5=hdf5-1.8.20

  # eharvey: TODO: remove COMPILER_ROOT and other exports below.
  export PATH=${COMPILER_ROOT}/bin:${PATH}
  export LD_LIBRARY_PATH=${COMPILER_ROOT}/lib:${LD_LIBRARY_PATH}
  export BINUTILS_ROOT=/usr/tce/packages/gcc/gcc-7.3.1
  export LIBRARY_PATH=${BINUTILS_ROOT}/lib
  export LIBRARY_PATH=${CBLAS_ROOT}/lib:${LIBRARY_PATH}
  export INCLUDE=${BINUTILS_ROOT}/include:${INCLUDE}
  export CPATH=${BINUTILS_ROOT}/include:${CPATH}

  if [[ "$ATDM_CONFIG_COMPILER" == "CUDA-10.1.243_"* ]]; then
    export LD_LIBRARY_PATH=${BINUTILS_ROOT}/rh/lib/gcc/ppc64le-redhat-linux/7:${LD_LIBRARY_PATH}
  fi

else
  echo
  echo "***"
  echo "*** ERROR: COMPILER=$ATDM_CONFIG_COMPILER is not supported on this system!"
  echo "***"
  return

fi

#
# Load module and do other setup for CUDA bulids
#

if [[ "$ATDM_CONFIG_COMPILER" == "CUDA-10.1.243_"* ]]; then

  sparc-dev/cuda-10.1.243_gcc-7.3.1_spmpi-rolling
  export CUDA_BIN_PATH=$CUDA_HOME

  # OpenMPI Settings
  # NOTE: the below export overrides the value set by the module load above
  export OMPI_CXX=${ATDM_CONFIG_NVCC_WRAPPER}
  if [ ! -x "$OMPI_CXX" ]; then
      echo "No nvcc_wrapper found"
      return
  fi

  # CUDA Settings
  if [[ ! -d /tmp/${USER} ]] ; then
    echo "Creating /tmp/${USER} for nvcc wrapper!"
    mkdir /tmp/${USER}
  fi
  # ATDM Settings
  export ATDM_CONFIG_USE_CUDA=ON
  export ATDM_CONFIG_USE_OPENMP=OFF
  export ATDM_CONFIG_USE_PTHREADS=OFF
  export ATDM_CONFIG_CTEST_PARALLEL_LEVEL=4

  # Kokkos Settings
  export ATDM_CONFIG_Kokkos_ENABLE_SERIAL=ON
  export KOKKOS_NUM_DEVICES=4

  # CTEST Settings
  # Trilinos_CTEST_RUN_CUDA_AWARE_MPI is used by cmake/ctest/driver/atdm/ats2/local-driver.sh
  export Trilinos_CTEST_RUN_CUDA_AWARE_MPI=1

elif [[ "$ATDM_CONFIG_COMPILER" == "CUDA"* ]]; then

  echo
  echo "***"
  echo "*** ERROR: CUDA version in COMPILER=$ATDM_CONFIG_COMPILER"
  echo "*** is not supported on this system!  Only CUDA-10.1.243"
  echo "*** is currently supported!"
  echo "***"
  return

fi

#
# Final setup for all build configurations
#

# Prepend path to ninja after all of the modules are loaded
export PATH=/projects/atdm_devops/vortex/ninja-fortran-1.8.2:$PATH

# Prepend path to updated CMake 3.16.5
module unload cmake
export PATH=/projects/atdm_devops/vortex/cmake-3.16.5/bin:$PATH

# ATDM specific config variables
export ATDM_CONFIG_LAPACK_LIBS="-L${LAPACK_ROOT}/lib;-llapack;-lgfortran;-lgomp"
export ATDM_CONFIG_BLAS_LIBS="-L${BLAS_ROOT}/lib;-lblas;-lgfortran;-lgomp;-lm"

# NOTE: Invalid libbfd.so requires below for Trilinos to compile
export ATDM_CONFIG_BINUTILS_LIBS="${BINUTILS_ROOT}/lib/libbfd.a;-lz;${BINUTILS_ROOT}/lib/libiberty.a"

export ATDM_CONFIG_USE_HWLOC=OFF
export ATDM_CONFIG_HDF5_LIBS="-L${HDF5_ROOT}/lib;${HDF5_ROOT}/lib/libhdf5_hl.a;${HDF5_ROOT}/lib/libhdf5.a;-lz;-ldl"
export ATDM_CONFIG_NETCDF_LIBS="-L${NETCDF_ROOT}/lib;${NETCDF_ROOT}/lib/libnetcdf.a;${PNETCDF_ROOT}/lib/libpnetcdf.a;${ATDM_CONFIG_HDF5_LIBS};-lcurl"

if [[ "${ATDM_CONFIG_SUPERLUDIST_INCLUDE_DIRS}" == "" ]] ; then
  export ATDM_CONFIG_SUPERLUDIST_INCLUDE_DIRS=${SUPERLUDIST_ROOT}/include
  export ATDM_CONFIG_SUPERLUDIST_LIBS="${SUPERLUDIST_ROOT}/lib64/libsuperlu_dist.a"
fi

# Set common MPI wrappers
export MPICC=`which mpicc`
export MPICXX=`which mpicxx`
export MPIF90=`which mpif90`

export ATDM_CONFIG_MPI_EXEC=${ATDM_SCRIPT_DIR}/ats2/trilinos_jsrun

export ATDM_CONFIG_MPI_POST_FLAGS="--rs_per_socket;4"
export ATDM_CONFIG_MPI_EXEC_NUMPROCS_FLAG="-p"

# System-info for what ATS-2 system we are using
if [[ "${ATDM_CONFIG_KNOWN_HOSTNAME}" == "vortex" ]] ; then
  export ATDM_CONFIG_ATS2_LOGIN_NODE=vortex60
  export ATDM_CONFIG_ATS2_LAUNCH_NODE=vortex59
  # NOTE: If more login and launch nodes gets added to 'vortex', we will need
  # to change this to a list of node names instead of just one.  But we will
  # deal with that later if that occurs.
else
  echo "Error, the ats2 env on system '${ATDM_CONFIG_KNOWN_HOSTNAME}'"
  return
fi


#
# Define functions for running on compute nodes
#


function atdm_ats2_get_allocated_compute_node_name() {
  if [[ "${LSB_HOSTS}" != "" ]] ; then
    echo ${LSB_HOSTS} | cut -d' ' -f2
  else
     echo
  fi
}
export -f atdm_ats2_get_allocated_compute_node_name


function atdm_ats2_get_node_type() {
  current_hostname=$(hostname)
  if   [[ "${current_hostname}" == "${ATDM_CONFIG_ATS2_LOGIN_NODE}" ]] ; then
    echo "login_node"
  elif [[ "${current_hostname}" == "${ATDM_CONFIG_ATS2_LAUNCH_NODE}" ]] ; then
    echo "launch_node"
  else
    echo "compute_node"
  fi
}
export -f atdm_ats2_get_node_type


#
# All done!
#

export ATDM_CONFIG_COMPLETED_ENV_SETUP=TRUE
