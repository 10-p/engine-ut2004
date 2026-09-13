# ArtifactDeps.cmake — the fetched-library provenance that meta.json records (ufront 2.70).
#
# Every third-party repository this build FetchContent's is declared as <repo>@<branch|tag>, NEVER as a
# commit: the project-owned forks (10-p/*) and our own repositories are consumed at their branch tip, so a
# rebuild picks up the latest of each ("everything is made for this project" — the owner's rule), and an
# external upstream stays on its release tag. meta.json's `toolchain.<lib>` says which repo and ref that
# was, fed from the SAME cache variables FetchContent_Declare read, so the descriptor cannot say one thing
# while the build fetched another. A local checkout override (FETCHCONTENT_SOURCE_DIR_<NAME>, or an
# engine-specific <LIB>_SOURCE_DIR) is recorded as `local:<path>`: an artifact built that way is not
# reproducible from the descriptor alone, and it must not pretend to be.
#
#   ufront_artifact_dep(<list-var> <label> <fetchcontent-name> <repo> <ref> [<local-source-dir>])
#     appends "--dep-<label>=<repo>@<ref>" (or "--dep-<label>=local:<dir>") to <list-var>, the argument
#     form cmake/emit-artifact-meta.mjs takes.
#
# ⚠ THE REBUILD TRAP THIS RELIES ON BEING CLOSED: FetchContent re-fetches a BRANCH ref on every CONFIGURE
# (FETCHCONTENT_UPDATES_DISCONNECTED is OFF, the default) and never on a plain `cmake --build`. Measured
# 2026-09-13: a populated _deps/utglr-src reset one commit behind its branch came back to the remote tip on
# the next `cmake` of the same build directory. publish-all configures before every build, so "latest from
# everything" holds there; a hand-run `cmake --build` on an old tree does not re-fetch, by design.
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
