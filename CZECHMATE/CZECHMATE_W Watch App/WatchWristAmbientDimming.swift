//
//  WatchWristAmbientDimming.swift
//  CZECHMATE_W — ztmavení UI ve shodě se systémem (AOD / sklopené zápěstí / neaktivní scéna).
//

import SwiftUI

/// Krycí vrstva + jemná úprava jasu, když systém sníží jas (Always On) nebo když scéna není aktivní.
private struct WatchWristAmbientDimmingModifier: ViewModifier {
    @Environment(\.scenePhase) private var scenePhase
    @Environment(\.isLuminanceReduced) private var isLuminanceReduced
    /// Bez prvního vykreslení může být `scenePhase`/`isLuminanceReduced` chybně „neaktivní“ → falešné ztmavení / pocit zaseknutého startu.
    @State private var rootHasAppeared = false
    /// Při startu z Xcode / přechodném stavu bývá `.inactive` — neztmavovat, dokud scéna jednou nebyla `.active`.
    @State private var hasBeenActiveOnce = false

    private var overlayOpacity: Double {
        guard rootHasAppeared else { return 0 }
        if isLuminanceReduced { return 0.5 }
        if !hasBeenActiveOnce {
            return 0
        }
        switch scenePhase {
        case .inactive:
            return 0.34
        case .background:
            return 0.58
        case .active:
            return 0
        @unknown default:
            return 0
        }
    }

    private var isDimmed: Bool { overlayOpacity > 0.01 }

    func body(content: Content) -> some View {
        content
            .overlay {
                if isDimmed {
                    Color.black
                        .opacity(overlayOpacity)
                        .ignoresSafeArea()
                        .allowsHitTesting(false)
                        .accessibilityHidden(true)
                }
            }
            .animation(.easeInOut(duration: 0.28), value: overlayOpacity)
            .onAppear {
                rootHasAppeared = true
            }
            .onChange(of: scenePhase) { _, phase in
                if phase == .active {
                    hasBeenActiveOnce = true
                }
            }
            .onChange(of: isDimmed) { _, new in
                #if DEBUG
                print(
                    "[Watch][STAGING] Ztmavení UI: \(new ? "zapnuto" : "vypnuto") (AOD=\(isLuminanceReduced), scenePhase=\(String(describing: scenePhase)))"
                )
                #endif
            }
    }
}

extension View {
    func watchWristAmbientDimming() -> some View {
        modifier(WatchWristAmbientDimmingModifier())
    }
}
