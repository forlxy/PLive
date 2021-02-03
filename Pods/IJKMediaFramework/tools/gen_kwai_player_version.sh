#! /bin/bash

writeVersionFile()
{
    filename=$1
    version=$2
    version_ext=$3
    
    echo "#ifndef __KWAI_PLAYER_VERSION_H__
#define __KWAI_PLAYER_VERSION_H__

// Auto-generated file. Please don't modify it manually.

#ifndef KWAI_PLAYER_VERSION
#define KWAI_PLAYER_VERSION (\"$version\")
#endif

#ifndef KWAI_PLAYER_VERSION_EXT
#define KWAI_PLAYER_VERSION_EXT (\"$version_ext\")
#endif

#endif // __KWAI_PLAYER_VERSION_H__
" > $filename;
}

local_path=$(cd "$(dirname $0)"; pwd)

ver_num_tag=$(git describe --tags --abbrev=0)
ver_num_sub=$(git rev-list --tags $ver_num_tag..HEAD --count)
commit_hash=$(git show --pretty=format:%h -s)
version=$ver_num_tag.$ver_num_sub.$commit_hash

branch=$(git rev-parse --abbrev-ref HEAD)
commit_date=$(git show --pretty=format:%ci -s)
version_ext=[$branch][$commit_date]

writeVersionFile "$local_path/../ijkmedia/ijkkwai/kwai_player_version_gennerated.h" "$version" "$version_ext"
echo version=$version
echo version_ext=$version_ext

