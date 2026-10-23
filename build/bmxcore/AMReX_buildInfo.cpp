namespace amrex {

const char* buildInfoGetBuildDate() {

  static const char BUILD_DATE[] = __DATE__ " "  __TIME__;
  return BUILD_DATE;
}

const char* buildInfoGetBuildDir() {

  static const char BUILD_DIR[] = "/Users/GLMarschmann/Projects/2024-LDRD-Fungi/BMX/build/src";
  return BUILD_DIR;
}

const char* buildInfoGetBuildMachine() {

  static const char BUILD_MACHINE[] = "macOS GLMarschmann-M29.local.vpn-dhcp.lbl.gov 26.3.1 25D771280a arm64";
  return BUILD_MACHINE;
}

const char* buildInfoGetAMReXDir() {

  static const char AMREX_DIR[] = "/Users/GLMarschmann/Projects/2024-LDRD-Fungi/BMX/subprojects/amrex";
  return AMREX_DIR;
}

const char* buildInfoGetComp() {

  static const char COMP[] = "AppleClang";
  return COMP;
}

const char* buildInfoGetCompVersion() {

  static const char COMP_VERSION[] = "21.0.0.21000101";
  return COMP_VERSION;
}

// deprecated
const char* buildInfoGetFcomp() {

  static const char FCOMP[] = "";
  return FCOMP;
}

// deprecated
const char* buildInfoGetFcompVersion() {

  static const char FCOMP_VERSION[] = "";
  return FCOMP_VERSION;
}

const char* buildInfoGetCXXName() {

  static const char CXX_comp_name[] = "/usr/bin/c++";
  return CXX_comp_name;
}

const char* buildInfoGetFName() {

  static const char F_comp_name[] = "";
  return F_comp_name;
}

const char* buildInfoGetCXXFlags() {

  static const char CXX_flags[] = "-DAMREX_SPACEDIM=3 -I/Users/GLMarschmann/Projects/2024-LDRD-Fungi/BMX/src -I/Users/GLMarschmann/Projects/2024-LDRD-Fungi/BMX/src/interpolation -I/Users/GLMarschmann/Projects/2024-LDRD-Fungi/BMX/src/deposition -I/Users/GLMarschmann/Projects/2024-LDRD-Fungi/BMX/src/des -I/Users/GLMarschmann/Projects/2024-LDRD-Fungi/BMX/src/diffusion -I/Users/GLMarschmann/Projects/2024-LDRD-Fungi/BMX/src/leveldata -I/Users/GLMarschmann/Projects/2024-LDRD-Fungi/BMX/src/setup -I/Users/GLMarschmann/Projects/2024-LDRD-Fungi/BMX/src/util -I/Users/GLMarschmann/Projects/2024-LDRD-Fungi/BMX/src/mods -I/Users/GLMarschmann/Projects/2024-LDRD-Fungi/BMX/src/timestepping -I/Users/GLMarschmann/Projects/2024-LDRD-Fungi/BMX/src/chemistry -I/Users/GLMarschmann/Projects/2024-LDRD-Fungi/BMX/subprojects/amrex/Src/Base -I/Users/GLMarschmann/Projects/2024-LDRD-Fungi/BMX/subprojects/amrex/Src/Base/Parser -I/Users/GLMarschmann/Projects/2024-LDRD-Fungi/BMX/subprojects/amrex/Src/Boundary -I/Users/GLMarschmann/Projects/2024-LDRD-Fungi/BMX/subprojects/amrex/Src/AmrCore -I/Users/GLMarschmann/Projects/2024-LDRD-Fungi/BMX/subprojects/amrex/Src/Amr -I/Users/GLMarschmann/Projects/2024-LDRD-Fungi/BMX/subprojects/amrex/Src/EB -I/Users/GLMarschmann/Projects/2024-LDRD-Fungi/BMX/subprojects/amrex/Src/LinearSolvers -I/Users/GLMarschmann/Projects/2024-LDRD-Fungi/BMX/subprojects/amrex/Src/LinearSolvers/MLMG -I/Users/GLMarschmann/Projects/2024-LDRD-Fungi/BMX/subprojects/amrex/Src/LinearSolvers/OpenBC -I/Users/GLMarschmann/Projects/2024-LDRD-Fungi/BMX/subprojects/amrex/Src/Particle -I/Users/GLMarschmann/Projects/2024-LDRD-Fungi/BMX/build/subprojects/amrex -O3 -DNDEBUG  -fno-math-errno";
  return CXX_flags;
}

const char* buildInfoGetFFlags() {

  static const char F_flags[] = "";
  return F_flags;
}

const char* buildInfoGetLinkFlags() {

  static const char link_flags[] = "";
  return link_flags;
}

const char* buildInfoGetLibraries() {

  static const char libraries[] = "Threads::Threads MPI::MPI_CXX";
  return libraries;
}

const char* buildInfoGetAux(int i) {

  //static const char AUX1[] = "${AUX[1]}";
  
  static const char EMPT[] = "";

  switch(i)
  {
    
    default: return EMPT;
  }
}

int buildInfoGetNumModules() {
  // int const num_modules = X;
  int const num_modules = 0;
  return num_modules;
}

const char* buildInfoGetModuleName(int i) {

  //static const char MNAME1[] = "${MNAME[1]}";
  
  static const char EMPT[] = "";

  switch(i)
  {
    
    default: return EMPT;
  }
}

const char* buildInfoGetModuleVal(int i) {

  //static const char MVAL1[] = "${MVAL[1]}";
  
  static const char EMPT[] = "";

  switch(i)
  {
    
    default: return EMPT;
  }
}

const char* buildInfoGetGitHash(int i) {

  //static const char HASH1[] = "${GIT[1]}";
  static const char HASH1[] = "d373eacdd559-dirty";
  static const char HASH2[] = "26.10";
  static const char EMPT[] = "";

  switch(i)
  {
    case 1: return HASH1;
    case 2: return HASH2;
    default: return EMPT;
  }
}

const char* buildInfoGetBuildGitHash() {

  //static const char HASH[] = "${GIT}";
  static const char HASH[] = "";

  return HASH;
}

const char* buildInfoGetBuildGitName() {

  //static const char NAME[] = "";
  static const char NAME[] = "";

  return NAME;
}

}
