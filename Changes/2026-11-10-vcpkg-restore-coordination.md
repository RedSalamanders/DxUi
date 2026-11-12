- Coordinate pinned vcpkg checkout/bootstrap changes and platform installs so concurrent
  architecture restores cannot clone or replace shared tooling under an active installer;
  retain parallel platform-private installs and never terminate another owner.
