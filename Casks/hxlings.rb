cask "hxlings" do
  version "0.1.5"
  sha256 "c3732bcc8a260efa4b6f1d38953d691dc9a9e3a5c499aec18f851f3d49273cf7"

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
