# GitHub Release

The GitHub release process is semi-automated, the GitHub Actions CI produces the artifacts but the release post
is created manually. This document describes the steps necessary to perform a release.

The version number is incremented on release, preferably the commit that changes the version number is the actual
release commit. The release commit must be tagged with the release version number. Since the GitHub workflows that
produce the artifacts only trigger on regular commits it is important to push that commit together with the tag,
otherwise the workflows need to be triggered again manually after the tag got pushed.

1. Verify that the wxFormBuilder Project File version is current. `ApplicationData::ApplicationData(const wxString&)`
   defined in `src/rad/appdata.cpp` should contain a comment if a version number increase is necessary.
   If an update is necessary, perform this in a separate commit. Update all internal wxFormBuilder project files
   and generate the code together in an additional commit.
2. Update the version number in the main `CMakeLists.txt` file in the root directory
3. Add the version number and release timestamp to the AppStream metadata file `data/platform/linux/share/metainfo/org.wxformbuilder.wxFormBuilder.metainfo.xml`
4. Add the version number and release timestamp to the Debian changelog file `data/packaging/linux/debian/changelog`
5. Add the version number and release timestamp to the changelog file `CHANGELOG.md`, update the links at the end of the file
6. Commit the changes
7. Tag the commit. The tag name must be the version number prefixed with the letter `v`. To make the tag annotated,
   add a simple message like `Release`.
8. Push the commit together with the tag: `git push --follow-tags`
9. Download all created artifacts and extract them. Not the downloaded zip archives but their content gets added to the release post.
   Rename the Flatpak file to contain the architecture tag like its archive, all other filenames are already final.
10. On the [GitHub](https://github.com/wxFormBuilder/wxFormBuilder/releases) website create a release for the tag.
    Generate the release message, copy the `Binaries` and `Source Code` sections from a previous release, replace the contents
    of the `What's Changed` section with the `CHANGELOG.md` content. Add any important information at the beginning of the release message.
11. Publish the release
12. Add the release to the [Wikidata](https://www.wikidata.org/wiki/Q4053267) entry of [wxFormBuilder](https://en.wikipedia.org/wiki/WxFormBuilder)
