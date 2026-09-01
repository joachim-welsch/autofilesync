// Renders the background artwork shown behind the icons in the AutoFileSync
// disk image. Run only when the artwork changes; the generated TIFF is checked
// in so that building the installer needs no Swift toolchain.
//   swift installer/make_dmg_background.swift installer
import AppKit

// Finder places icons in a top-left coordinate system, AppKit draws bottom-up;
// the layout constants below are the Finder ones the installer script also uses.
let windowWidth = 600.0, windowHeight = 400.0
let iconRow = 205.0, sourceIcon = 150.0, targetIcon = 450.0

func flip(_ yFromTop: Double) -> Double { windowHeight - yFromTop }

func color(_ hex: UInt32) -> NSColor {
    NSColor(srgbRed: Double((hex >> 16) & 0xff) / 255, green: Double((hex >> 8) & 0xff) / 255,
            blue: Double(hex & 0xff) / 255, alpha: 1)
}

func drawArrow() {
    let head = 18.0, shaftEnd = targetIcon - 100 - head, shaft = NSBezierPath()
    shaft.move(to: NSPoint(x: sourceIcon + 100, y: flip(iconRow)))
    shaft.line(to: NSPoint(x: shaftEnd, y: flip(iconRow)))
    shaft.lineWidth = 6
    shaft.lineCapStyle = .round
    color(0xb0b0b6).setStroke()
    shaft.stroke()
    let tip = NSBezierPath()
    tip.move(to: NSPoint(x: shaftEnd + head, y: flip(iconRow)))
    tip.line(to: NSPoint(x: shaftEnd, y: flip(iconRow) + head * 0.75))
    tip.line(to: NSPoint(x: shaftEnd, y: flip(iconRow) - head * 0.75))
    tip.close()
    color(0xb0b0b6).setFill()
    tip.fill()
}

func drawCentred(_ text: String, font: NSFont, colour: NSColor, yFromTop: Double) {
    let style = NSMutableParagraphStyle()
    style.alignment = .center
    let attributes: [NSAttributedString.Key: Any] = [.font: font, .foregroundColor: colour, .paragraphStyle: style]
    let height = font.ascender - font.descender
    text.draw(in: NSRect(x: 0, y: flip(yFromTop) - height, width: windowWidth, height: height), withAttributes: attributes)
}

func render(scale: Double) -> Data {
    let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: Int(windowWidth * scale),
                              pixelsHigh: Int(windowHeight * scale), bitsPerSample: 8, samplesPerPixel: 4,
                              hasAlpha: true, isPlanar: false, colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
    rep.size = NSSize(width: windowWidth, height: windowHeight)
    NSGraphicsContext.saveGraphicsState()
    NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: rep)
    NSGradient(starting: color(0xfbfbfc), ending: color(0xe9e9ed))!
        .draw(in: NSRect(x: 0, y: 0, width: windowWidth, height: windowHeight), angle: -90)
    drawArrow()
    drawCentred("AutoFileSync installieren", font: .systemFont(ofSize: 21, weight: .semibold), colour: color(0x2f2f31), yFromTop: 58)
    drawCentred("AutoFileSync auf „Applications“ ziehen", font: .systemFont(ofSize: 13), colour: color(0x6e6e73), yFromTop: 92)
    drawCentred("Danach das Programm aus dem Programme-Ordner starten.", font: .systemFont(ofSize: 11), colour: color(0x8e8e93), yFromTop: 352)
    NSGraphicsContext.restoreGraphicsState()
    return rep.representation(using: .png, properties: [:])!
}

let outputDir = CommandLine.arguments.count > 1 ? CommandLine.arguments[1] : "."
for (scale, name) in [(1.0, "dmg-background.png"), (2.0, "dmg-background@2x.png")] {
    let url = URL(fileURLWithPath: outputDir).appendingPathComponent(name)
    try! render(scale: scale).write(to: url)
    print("wrote \(url.path)")
}
