// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "CZECHMATEShared",
    platforms: [.iOS(.v17), .watchOS(.v10), .macOS(.v14)],
    products: [
        .library(name: "CZECHMATEShared", targets: ["CZECHMATEShared"]),
    ],
    targets: [
        .target(
            name: "CZECHMATEShared",
            dependencies: [],
            path: "Sources/CZECHMATEShared"
        ),
    ]
)
