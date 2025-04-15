The libcgroup Release Process
===============================================================================
https://github.com/libcgroup/libcgroup

This is the process that should be followed when creating a new libcgroup
release.

#### 1. Verify that all issues assigned to the release milestone have been resolved

  * https://github.com/libcgroup/libcgroup/milestones

#### 2. Verify that the Github Actions are all passing

#### 3. Verify that the bundled test suite runs without error

	# ./bootstrap.sh
	# make check

#### 4. Verify that the packaging is correct

	# make distcheck

#### 5. Verify that there are no outstanding defects from Coverity

    # ./bootstrap.sh
    # export PATH=$PATH:/path/to/cov-build
    # cov-build --dir cov-int make
    # # verify the build succeeded.  Examine $? and cov-int/build-log.txt
    # tar czvf libcgroup-main@<current hash>.tar.gz cov-int
    # Upload the tar file to Coverity

#### 6. Perform any distribution test builds

  * Oracle Linux
  * Fedora Rawhide
  * Red Hat Enterprise Linux
  * etc.

#### 7. If any problems were found up to this point that resulted in code changes, restart the process

#### 8. If this is a new major/minor release, create new 'release-X.Y' branch

	# git branch "release-X.Y"

#### 9. Update and commit the version number in configure.ac AC_INIT(...) macro and LIBRARY version macros

#### 10. Tag the release in the local repository with a signed tag

	# git tag -s -m "version X.Y.Z" vX.Y.Z

#### 11. Build final release tarball

	# make clean
	# ./bootstrap.sh
	# make dist-gzip

#### 12. Verify the release tarball in a separate directory

	<unpack the release tarball in a temporary directory>
	# ./configure --sysconfdir=/etc --localstatedir=/var \
	--enable-opaque-hierarchy="name=systemd" --enable-python --enable-systemd
	# make check

#### 13. Generate a checksum for the release tarball

	# sha256sum <tarball> > libcgroup-X.Y.Z.tar.gz.SHA256SUM

#### 14. GPG sign the release tarball and checksum using the maintainer's key

	# gpg --armor --detach-sign libcgroup-X.Y.Z.tar.gz
	# gpg --clearsign libcgroup-X.Y.Z.tar.gz.SHA256SUM

#### 15. Push the release tag to the main GitHub repository

	# git push <repo> vX.Y.Z

#### 16. Push the release branch to the main GitHub repository

    # git push <repo> release-X.Y

#### 17. Create a new GitHub release using the associated tag and upload the following files

  * libcgroup-X.Y.Z.tar.gz
  * libcgroup-X.Y.Z.tar.gz.asc
  * libcgroup-X.Y.Z.tar.gz.SHA256SUM
  * libcgroup-X.Y.Z.tar.gz.SHA256SUM.asc

#### 18. Update the GitHub release notes for older releases which are now unsupported

The following Markdown text is suggested at the top of the release note, see old GitHub releases for examples.

```
***This release is no longer supported upstream, please use a more recent release***
```
