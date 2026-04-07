//
//  CZECHMATEWidgetsBundle.swift
//

import SwiftUI
import WidgetKit

@main
struct CZECHMATEWidgetsBundle: WidgetBundle {
    var body: some Widget {
        ChessMatchLiveActivity()
        ChessLinkWidget()
    }
}
