//
//  RootWatchView.swift
//  CZECHMATE_W — první spuštění: úvod, potom hlavní UI.
//

import SwiftUI

struct RootWatchView: View {
    @AppStorage("czechmate.watch.onboarding.completed") private var onboardingCompleted = false

    var body: some View {
        Group {
            if onboardingCompleted {
                ContentView()
                    .environment(\.watchIntroReset) {
                        onboardingCompleted = false
                    }
            } else {
                WatchOnboardingView {
                    // main.async: po dokončení akce tlačítka a před startem BLE v ContentView — spolehlivější než Task při zatíženém MainActoru.
                    WatchAppLog.staging("Onboarding dokončen → přepínám na hlavní UI")
                    DispatchQueue.main.async {
                        onboardingCompleted = true
                        WatchAppLog.staging("onboardingCompleted = true")
                    }
                }
            }
        }
        .watchWristAmbientDimming()
        .onAppear {
            WatchAppLog.staging("RootWatchView onAppear, onboardingCompleted=\(onboardingCompleted)")
        }
    }
}
