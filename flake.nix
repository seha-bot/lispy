{
  inputs.nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";

  outputs =
    { nixpkgs, ... }:
    let
      pkgs = nixpkgs.legacyPackages.x86_64-linux;
    in
    {
      devShell.x86_64-linux = pkgs.mkShell.override { stdenv = pkgs.clangStdenv; } {
        nativeBuildInputs = [
          pkgs.catch2_3
          pkgs.clang-tools
          pkgs.cmake
          pkgs.ninja
        ];
      };
    };
}
