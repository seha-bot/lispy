{
  description = "A flake for building the lispy compiler";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
  };

  outputs =
    { nixpkgs, ... }:
    let
      pkgs = nixpkgs.legacyPackages.x86_64-linux;
    in
    {
      devShell.x86_64-linux = pkgs.mkShell {
        nativeBuildInputs = [
          pkgs.clang-tools
          pkgs.clang
          pkgs.cmake
          pkgs.ninja
        ];
        packages = with pkgs; [
          pkg-config
          readline
        ];
      };
    };
}
