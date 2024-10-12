# 设置C++标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (PROJECT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(CXXFLAGS)
    if (MSVC)
        list(APPEND CXXFLAGS -utf-8 -Zc:preprocessor -permissive- -EHsc)
        add_definitions(-DNOMINMAX)
        if (CO_ASYNC_WARN)
            list(APPEND CXXFLAGS -W4)
        endif()
    else()
        if (CO_ASYNC_WARN)
            list(APPEND CXXFLAGS -Wall -Wextra -Wno-interference-size -Werror=return-type
            -Werror=unused-result -Werror=uninitialized -Werror=maybe-uninitialized
            -Wno-unused-parameter -Wunused-but-set-variable -Wunused-but-set-parameter -Wunused-function
            -Wunused-const-variable -Werror=use-after-free=3 -Werror=invalid-memory-model -Wunused-value
            -Wexceptions -Werror=missing-declarations -Werror=missing-field-initializers
            -Wparentheses -Wconversion -Werror=float-conversion -Wsign-conversion -Wsign-compare -Wno-terminate
            -Werror=delete-non-virtual-dtor -Werror=suggest-override -Wsign-promo -Wold-style-cast
            -Wrange-loop-construct -Werror=reorder -Werror=class-memaccess -Werror=narrowing
            -Wdeprecated-enum-enum-conversion -Werror=deprecated-copy-dtor -Werror=deprecated-copy
            -Wzero-as-null-pointer-constant -Wplacement-new=2 -Wsuggest-final-types -Wsuggest-final-methods
            -Werror=conversion-null -Werror=mismatched-new-delete -Werror=address -Wlogical-op -Wlogical-not-parentheses
            -Wattributes -Wmissing-declarations -Werror=multichar -Werror=overflow -Werror=restrict -Werror=vla
            -Wstrict-aliasing=1 -Werror=string-compare -Werror=stringop-overflow=2 -Werror=shift-overflow
            -Wsuggest-attribute=const -Wsuggest-attribute=pure -Wsuggest-attribute=noreturn -Werror=alloca
            -Werror=array-bounds -Werror=bool-compare -Werror=bool-operation -Werror=zero-length-bounds
            -Werror=div-by-zero -Wno-shadow -Werror=format -Werror=pointer-arith -Werror=write-strings
            -Werror=dangling-pointer=2 -Werror=return-local-addr -Wempty-body -Wimplicit-fallthrough
            -Wswitch -Wno-unknown-warning-option)
        elseif(CO_ASYNC_DEBUG)
            list(APPEND CXXFLAGS -Wall -Wextra -Wno-interference-size -Wreturn-type -Wno-unused-parameter )
        endif()
        if (CO_ASYNC_DEBUG)
            list(APPEND CXXFLAGS -Wno-exceptions)
        else()
            if (CMAKE_BUILD_TYPE MATCHES "[Rr][Ee][Ll][Ee][Aa][Ss][Ee]")
                list(APPEND CXXFLAGS -flto)
            endif()
        endif()
    endif()
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 10.0.0 AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 11.0.0)
        #add_definitions(-D__cplusplus=202002L)
        list(APPEND CXXFLAGS -fcoroutines)
        if (IS_DIRECTORY /usr/include/c++/10)
            include_directories($<$<COMPILE_LANG_AND_ID:CXX,GNU>:/usr/include/c++/10>)
        endif()
        if (CO_ASYNC_WARN)
            list(APPEND CXXFLAGS -Wno-zero-as-null-pointer-constant -Wno-unused-const-variable)
        endif()
        list(APPEND CXXFLAGS -Wno-attributes)
        if (CO_ASYNC_DEBUG)
            list(APPEND CXXFLAGS -Wno-terminate)
        endif()
    else()
        if (CO_ASYNC_WARN)
            list(APPEND CXXFLAGS -Werror)
        endif()
    endif()
endif()
