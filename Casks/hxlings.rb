cask "hxlings" do
  version "0.1.6"
  sha256 "7ec6acfd52ca75bacb645a00adaee5d65801657b8f48acca58a13f26ec9d99b5"

  url "https://github.com/FarnaHerry/Hxlings/releases/download/v#{version}/hxlings-#{version}-macos-arm64.dmg"
  name "Hxlings"
  desc "Desktop GUI manager for xlings built with mcpp and HuxerUI"
  homepage "https://github.com/FarnaHerry/Hxlings"

  depends_on arch: :arm64

  app "Hxlings.app"

  zap trash: [
    "~/Library/Application Support/hxlings",
    "~/Library/Preferences/io.github.farnaherry.hxlings.plist",
  ]

  caveats <<~EOS
    Hxlings is currently ad-hoc signed (not notarized with an Apple Developer ID).
    Before first launch (or if macOS Gatekeeper blocks opening Hxlings), clear the
    quarantine attribute manually:

      xattr -dr com.apple.quarantine /Applications/Hxlings.app
  EOS
end
