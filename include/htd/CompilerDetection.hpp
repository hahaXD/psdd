
// This is a generated file. Do not edit!

#ifndef HTD_COMPILER_DETECTION_H
#define HTD_COMPILER_DETECTION_H

#ifdef __cplusplus
# define HTD_COMPILER_IS_Comeau 0
# define HTD_COMPILER_IS_Intel 0
# define HTD_COMPILER_IS_PathScale 0
# define HTD_COMPILER_IS_Embarcadero 0
# define HTD_COMPILER_IS_Borland 0
# define HTD_COMPILER_IS_Watcom 0
# define HTD_COMPILER_IS_OpenWatcom 0
# define HTD_COMPILER_IS_SunPro 0
# define HTD_COMPILER_IS_HP 0
# define HTD_COMPILER_IS_Compaq 0
# define HTD_COMPILER_IS_zOS 0
# define HTD_COMPILER_IS_XLClang 0
# define HTD_COMPILER_IS_XL 0
# define HTD_COMPILER_IS_VisualAge 0
# define HTD_COMPILER_IS_PGI 0
# define HTD_COMPILER_IS_Cray 0
# define HTD_COMPILER_IS_TI 0
# define HTD_COMPILER_IS_Fujitsu 0
# define HTD_COMPILER_IS_GHS 0
# define HTD_COMPILER_IS_SCO 0
# define HTD_COMPILER_IS_ARMCC 0
# define HTD_COMPILER_IS_AppleClang 0
# define HTD_COMPILER_IS_ARMClang 0
# define HTD_COMPILER_IS_Clang 0
# define HTD_COMPILER_IS_GNU 0
# define HTD_COMPILER_IS_MSVC 0
# define HTD_COMPILER_IS_ADSP 0
# define HTD_COMPILER_IS_IAR 0
# define HTD_COMPILER_IS_MIPSpro 0

#if defined(__COMO__)
# undef HTD_COMPILER_IS_Comeau
# define HTD_COMPILER_IS_Comeau 1

#elif defined(__INTEL_COMPILER) || defined(__ICC)
# undef HTD_COMPILER_IS_Intel
# define HTD_COMPILER_IS_Intel 1

#elif defined(__PATHCC__)
# undef HTD_COMPILER_IS_PathScale
# define HTD_COMPILER_IS_PathScale 1

#elif defined(__BORLANDC__) && defined(__CODEGEARC_VERSION__)
# undef HTD_COMPILER_IS_Embarcadero
# define HTD_COMPILER_IS_Embarcadero 1

#elif defined(__BORLANDC__)
# undef HTD_COMPILER_IS_Borland
# define HTD_COMPILER_IS_Borland 1

#elif defined(__WATCOMC__) && __WATCOMC__ < 1200
# undef HTD_COMPILER_IS_Watcom
# define HTD_COMPILER_IS_Watcom 1

#elif defined(__WATCOMC__)
# undef HTD_COMPILER_IS_OpenWatcom
# define HTD_COMPILER_IS_OpenWatcom 1

#elif defined(__SUNPRO_CC)
# undef HTD_COMPILER_IS_SunPro
# define HTD_COMPILER_IS_SunPro 1

#elif defined(__HP_aCC)
# undef HTD_COMPILER_IS_HP
# define HTD_COMPILER_IS_HP 1

#elif defined(__DECCXX)
# undef HTD_COMPILER_IS_Compaq
# define HTD_COMPILER_IS_Compaq 1

#elif defined(__IBMCPP__) && defined(__COMPILER_VER__)
# undef HTD_COMPILER_IS_zOS
# define HTD_COMPILER_IS_zOS 1

#elif defined(__ibmxl__) && defined(__clang__)
# undef HTD_COMPILER_IS_XLClang
# define HTD_COMPILER_IS_XLClang 1

#elif defined(__IBMCPP__) && !defined(__COMPILER_VER__) && __IBMCPP__ >= 800
# undef HTD_COMPILER_IS_XL
# define HTD_COMPILER_IS_XL 1

#elif defined(__IBMCPP__) && !defined(__COMPILER_VER__) && __IBMCPP__ < 800
# undef HTD_COMPILER_IS_VisualAge
# define HTD_COMPILER_IS_VisualAge 1

#elif defined(__PGI)
# undef HTD_COMPILER_IS_PGI
# define HTD_COMPILER_IS_PGI 1

#elif defined(_CRAYC)
# undef HTD_COMPILER_IS_Cray
# define HTD_COMPILER_IS_Cray 1

#elif defined(__TI_COMPILER_VERSION__)
# undef HTD_COMPILER_IS_TI
# define HTD_COMPILER_IS_TI 1

#elif defined(__FUJITSU) || defined(__FCC_VERSION) || defined(__fcc_version)
# undef HTD_COMPILER_IS_Fujitsu
# define HTD_COMPILER_IS_Fujitsu 1

#elif defined(__ghs__)
# undef HTD_COMPILER_IS_GHS
# define HTD_COMPILER_IS_GHS 1

#elif defined(__SCO_VERSION__)
# undef HTD_COMPILER_IS_SCO
# define HTD_COMPILER_IS_SCO 1

#elif defined(__ARMCC_VERSION) && !defined(__clang__)
# undef HTD_COMPILER_IS_ARMCC
# define HTD_COMPILER_IS_ARMCC 1

#elif defined(__clang__) && defined(__apple_build_version__)
# undef HTD_COMPILER_IS_AppleClang
# define HTD_COMPILER_IS_AppleClang 1

#elif defined(__clang__) && defined(__ARMCOMPILER_VERSION)
# undef HTD_COMPILER_IS_ARMClang
# define HTD_COMPILER_IS_ARMClang 1

#elif defined(__clang__)
# undef HTD_COMPILER_IS_Clang
# define HTD_COMPILER_IS_Clang 1

#elif defined(__GNUC__) || defined(__GNUG__)
# undef HTD_COMPILER_IS_GNU
# define HTD_COMPILER_IS_GNU 1

#elif defined(_MSC_VER)
# undef HTD_COMPILER_IS_MSVC
# define HTD_COMPILER_IS_MSVC 1

#elif defined(__VISUALDSPVERSION__) || defined(__ADSPBLACKFIN__) || defined(__ADSPTS__) || defined(__ADSP21000__)
# undef HTD_COMPILER_IS_ADSP
# define HTD_COMPILER_IS_ADSP 1

#elif defined(__IAR_SYSTEMS_ICC__) || defined(__IAR_SYSTEMS_ICC)
# undef HTD_COMPILER_IS_IAR
# define HTD_COMPILER_IS_IAR 1


#endif

#  if HTD_COMPILER_IS_GNU

#    if !((__GNUC__ * 100 + __GNUC_MINOR__) >= 404)
#      error Unsupported compiler version
#    endif

# if defined(__GNUC__)
#  define HTD_COMPILER_VERSION_MAJOR (__GNUC__)
# else
#  define HTD_COMPILER_VERSION_MAJOR (__GNUG__)
# endif
# if defined(__GNUC_MINOR__)
#  define HTD_COMPILER_VERSION_MINOR (__GNUC_MINOR__)
# endif
# if defined(__GNUC_PATCHLEVEL__)
#  define HTD_COMPILER_VERSION_PATCH (__GNUC_PATCHLEVEL__)
# endif

#    if (__GNUC__ * 100 + __GNUC_MINOR__) >= 406 && (__cplusplus >= 201103L || (defined(__GXX_EXPERIMENTAL_CXX0X__) && __GXX_EXPERIMENTAL_CXX0X__))
#      define HTD_COMPILER_CXX_NOEXCEPT 1
#    else
#      define HTD_COMPILER_CXX_NOEXCEPT 0
#    endif

#    if (__GNUC__ * 100 + __GNUC_MINOR__) >= 407 && __cplusplus >= 201103L
#      define HTD_COMPILER_CXX_OVERRIDE 1
#    else
#      define HTD_COMPILER_CXX_OVERRIDE 0
#    endif

#    if (__GNUC__ * 100 + __GNUC_MINOR__) >= 405 && (__cplusplus >= 201103L || (defined(__GXX_EXPERIMENTAL_CXX0X__) && __GXX_EXPERIMENTAL_CXX0X__))
#      define HTD_COMPILER_CXX_LAMBDAS 1
#    else
#      define HTD_COMPILER_CXX_LAMBDAS 0
#    endif

#  elif HTD_COMPILER_IS_Clang

#    if !(((__clang_major__ * 100) + __clang_minor__) >= 301)
#      error Unsupported compiler version
#    endif

# define HTD_COMPILER_VERSION_MAJOR (__clang_major__)
# define HTD_COMPILER_VERSION_MINOR (__clang_minor__)
# define HTD_COMPILER_VERSION_PATCH (__clang_patchlevel__)
# if defined(_MSC_VER)
   /* _MSC_VER = VVRR */
#  define HTD_SIMULATE_VERSION_MAJOR (_MSC_VER / 100)
#  define HTD_SIMULATE_VERSION_MINOR (_MSC_VER % 100)
# endif

#    if ((__clang_major__ * 100) + __clang_minor__) >= 301 && __has_feature(cxx_noexcept)
#      define HTD_COMPILER_CXX_NOEXCEPT 1
#    else
#      define HTD_COMPILER_CXX_NOEXCEPT 0
#    endif

#    if ((__clang_major__ * 100) + __clang_minor__) >= 301 && __has_feature(cxx_override_control)
#      define HTD_COMPILER_CXX_OVERRIDE 1
#    else
#      define HTD_COMPILER_CXX_OVERRIDE 0
#    endif

#    if ((__clang_major__ * 100) + __clang_minor__) >= 301 && __has_feature(cxx_lambdas)
#      define HTD_COMPILER_CXX_LAMBDAS 1
#    else
#      define HTD_COMPILER_CXX_LAMBDAS 0
#    endif

#  elif HTD_COMPILER_IS_AppleClang

#    if !(((__clang_major__ * 100) + __clang_minor__) >= 400)
#      error Unsupported compiler version
#    endif

# define HTD_COMPILER_VERSION_MAJOR (__clang_major__)
# define HTD_COMPILER_VERSION_MINOR (__clang_minor__)
# define HTD_COMPILER_VERSION_PATCH (__clang_patchlevel__)
# if defined(_MSC_VER)
   /* _MSC_VER = VVRR */
#  define HTD_SIMULATE_VERSION_MAJOR (_MSC_VER / 100)
#  define HTD_SIMULATE_VERSION_MINOR (_MSC_VER % 100)
# endif
# define HTD_COMPILER_VERSION_TWEAK (__apple_build_version__)

#    if ((__clang_major__ * 100) + __clang_minor__) >= 400 && __has_feature(cxx_noexcept)
#      define HTD_COMPILER_CXX_NOEXCEPT 1
#    else
#      define HTD_COMPILER_CXX_NOEXCEPT 0
#    endif

#    if ((__clang_major__ * 100) + __clang_minor__) >= 400 && __has_feature(cxx_override_control)
#      define HTD_COMPILER_CXX_OVERRIDE 1
#    else
#      define HTD_COMPILER_CXX_OVERRIDE 0
#    endif

#    if ((__clang_major__ * 100) + __clang_minor__) >= 400 && __has_feature(cxx_lambdas)
#      define HTD_COMPILER_CXX_LAMBDAS 1
#    else
#      define HTD_COMPILER_CXX_LAMBDAS 0
#    endif

#  elif HTD_COMPILER_IS_MSVC

#    if !(_MSC_VER >= 1600)
#      error Unsupported compiler version
#    endif

  /* _MSC_VER = VVRR */
# define HTD_COMPILER_VERSION_MAJOR (_MSC_VER / 100)
# define HTD_COMPILER_VERSION_MINOR (_MSC_VER % 100)
# if defined(_MSC_FULL_VER)
#  if _MSC_VER >= 1400
    /* _MSC_FULL_VER = VVRRPPPPP */
#   define HTD_COMPILER_VERSION_PATCH (_MSC_FULL_VER % 100000)
#  else
    /* _MSC_FULL_VER = VVRRPPPP */
#   define HTD_COMPILER_VERSION_PATCH (_MSC_FULL_VER % 10000)
#  endif
# endif
# if defined(_MSC_BUILD)
#  define HTD_COMPILER_VERSION_TWEAK (_MSC_BUILD)
# endif

#    if _MSC_VER >= 1900
#      define HTD_COMPILER_CXX_NOEXCEPT 1
#    else
#      define HTD_COMPILER_CXX_NOEXCEPT 0
#    endif

#    if _MSC_VER >= 1600
#      define HTD_COMPILER_CXX_OVERRIDE 1
#    else
#      define HTD_COMPILER_CXX_OVERRIDE 0
#    endif

#    if _MSC_VER >= 1600
#      define HTD_COMPILER_CXX_LAMBDAS 1
#    else
#      define HTD_COMPILER_CXX_LAMBDAS 0
#    endif

#  else
#    error Unsupported compiler
#  endif

#  if defined(HTD_COMPILER_CXX_NOEXCEPT) && HTD_COMPILER_CXX_NOEXCEPT
#    define HTD_NOEXCEPT noexcept
#    define HTD_NOEXCEPT_EXPR(X) noexcept(X)
#  else
#    define HTD_NOEXCEPT
#    define HTD_NOEXCEPT_EXPR(X)
#  endif


#  if defined(HTD_COMPILER_CXX_OVERRIDE) && HTD_COMPILER_CXX_OVERRIDE
#    define HTD_OVERRIDE override
#  else
#    define HTD_OVERRIDE 
#  endif

#endif

#endif
