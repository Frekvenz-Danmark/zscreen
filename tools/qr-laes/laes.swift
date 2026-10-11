import Foundation
import Vision
import CoreImage

let sti = CommandLine.arguments.count > 1 ? CommandLine.arguments[1] : "qr.bmp"
guard let img = CIImage(contentsOf: URL(fileURLWithPath: sti)) else {
    print("kunne ikke laese billedet"); exit(2)
}
let req = VNDetectBarcodesRequest()
req.symbologies = [.qr]
let h = VNImageRequestHandler(ciImage: img, options: [:])
do { try h.perform([req]) } catch { print("fejl: \(error)"); exit(2) }
guard let res = req.results, !res.isEmpty else { print("INGEN QR FUNDET"); exit(1) }
for r in res {
    print("laest: \(r.payloadStringValue ?? "(ingen tekst)")  symbologi: \(r.symbology.rawValue)")
}
