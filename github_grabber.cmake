include(FetchContent)

function(fetch_github_library)
    # Named arguments
    cmake_parse_arguments(
            FGH
            ""
            "NAME;GIT_REPOSITORY;GIT_TAG"
            ""
            ${ARGN}
    )

    if (NOT FGH_NAME)
        message(FATAL_ERROR "fetch_github_library requires NAME")
    endif()

    if (NOT FGH_GIT_REPOSITORY)
        message(FATAL_ERROR "fetch_github_library requires GIT_REPOSITORY")
    endif()

    FetchContent_Declare(
            ${FGH_NAME}
            GIT_REPOSITORY ${FGH_GIT_REPOSITORY}
            GIT_TAG        ${FGH_GIT_TAG}
    )

    FetchContent_MakeAvailable(${FGH_NAME})








endfunction()