# ArtifactDeps.cmake — the fetched-library provenance that meta.json records.
#
# Every third-party repository this build FetchContent's is declared as <repo>@<branch|tag>, never as a
# commit: the project-owned forks (10-p/*) and the project's own repositories are consumed at their branch
# tip, so a rebuild picks up the latest of each, and an external upstream stays on its release tag.
# meta.json's `toolchain.<lib>` says which repo and ref that was, fed from the same cache variables
# FetchContent_Declare read, so the descriptor cannot say one thing while the build fetched another. A
# local checkout override (FETCHCONTENT_SOURCE_DIR_<NAME>, or an engine-specific <LIB>_SOURCE_DIR) is
# recorded as `local:<path>`: an artifact built that way is not reproducible from the descriptor alone.
#
#   ufront_artifact_dep(<list-var> <label> <fetchcontent-name> <repo> <ref> [<local-source-dir>])
#     appends "--dep-<label>=<repo>@<ref>" (or "--dep-<label>=local:<dir>") to <list-var>, the argument
#     form Source/SDLLaunch/cmake/emit-artifact-meta.mjs takes.
#
# FetchContent re-fetches a branch ref on every configure (FETCHCONTENT_UPDATES_DISCONNECTED is OFF, the
# default) and never on a plain `cmake --build`; publish-all configures before every build.
function(ufront_artifact_dep out label fcname repo ref)
  string(TOUPPER "${fcname}" _u)
  set(_local "${FETCHCONTENT_SOURCE_DIR_${_u}}")
  if(NOT _local AND ARGC GREATER 5)
    set(_local "${ARGV5}")
  endif()
  if(_local)
    set(_spec "local:${_local}")
  else()
    set(_spec "${repo}@${ref}")
  endif()
  list(APPEND ${out} "--dep-${label}=${_spec}")
  set(${out} "${${out}}" PARENT_SCOPE)
endfunction()
