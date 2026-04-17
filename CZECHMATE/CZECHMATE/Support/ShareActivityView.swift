//
//  ShareActivityView.swift
//  CZECHMATE — systémové sdílení (UIImage, text, …) přes UIActivityViewController.
//

import SwiftUI
import UIKit

struct ShareActivityView: UIViewControllerRepresentable {
    var items: [Any]
    var applicationActivities: [UIActivity]? = nil

    func makeUIViewController(context: Context) -> UIActivityViewController {
        UIActivityViewController(activityItems: items, applicationActivities: applicationActivities)
    }

    func updateUIViewController(_ uiViewController: UIActivityViewController, context: Context) {}
}
