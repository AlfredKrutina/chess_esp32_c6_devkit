//
//  SoundManager.swift
//  CZECHMATE — zvuky tahů: WAV v balíčku (Kenney UI Audio, CC0), styl podobný populárním webovým šachům.
//
//  Pozn.: Přesné samply Chess.com nelze přebírat (vlastnická práva). Použité zvuky jsou CC0 a lze je vyměnit
//  za vlastní .wav se stejnými názvy v Resources/ChessSounds/.
//

import AudioToolbox
import AVFoundation
import Foundation
import CZECHMATEShared

/// Centrální přehrávání zvuků šachovnice.
@MainActor
final class SoundManager {
    static let shared = SoundManager()

    /// Musí odpovídat `@AppStorage("czechmate.soundEffectsEnabled")` v Nastavení.
    private static let enabledKey = "czechmate.soundEffectsEnabled"

    private var isEnabled: Bool {
        if UserDefaults.standard.object(forKey: Self.enabledKey) == nil { return true }
        return UserDefaults.standard.bool(forKey: Self.enabledKey)
    }

    private var sessionConfigured = false
    private var players: [String: AVAudioPlayer] = [:]

    private init() {}

    // MARK: - Veřejné API (jednotlivé akce / UI)

    func playMoveSound() {
        playBundled("ChessMove", fallbackSystem: 1104)
    }

    func playCaptureSound() {
        playBundled("ChessCapture", fallbackSystem: 1057)
    }

    func playCheckSound() {
        playBundled("ChessCheck", fallbackSystem: 1520)
    }

    func playMateSound() {
        playBundled("ChessMate", fallbackSystem: 1520)
    }

    func playCastleSound() {
        playBundled("ChessCastle", fallbackSystem: 1104)
    }

    func playPromoteSound() {
        playBundled("ChessPromote", fallbackSystem: 1104)
    }

    func playPieceSelectSound() {
        playBundled("ChessSelect", fallbackSystem: 1123)
    }

    func playInvalidMoveSound() {
        playBundled("ChessIllegal", fallbackSystem: 1073)
    }

    /// Po novém snímku z desky: vhodný zvuk podle typu tahu (mat, rošáda, braní, promoce, šach, klidný tah).
    func playFeedbackAfterBoardMove(previous: GameSnapshot, current: GameSnapshot) {
        guard current.history.moves.count == previous.history.moves.count + 1,
              let last = current.history.moves.last else { return }

        let wDelta = current.captured.whiteCaptured.count - previous.captured.whiteCaptured.count
        let bDelta = current.captured.blackCaptured.count - previous.captured.blackCaptured.count
        let isCapture = wDelta > 0 || bDelta > 0

        if current.status.checkmate == true {
            playMateSound()
            #if os(iOS)
            HapticSettings.heavyImpactIfEnabled()
            #endif
            return
        }

        if current.status.stalemate == true {
            playMoveSound()
            return
        }

        if Self.isCastling(last) {
            playCastleSound()
            #if os(iOS)
            HapticSettings.mediumImpactIfEnabled()
            #endif
            return
        }

        if isCapture {
            playCaptureSound()
            #if os(iOS)
            HapticSettings.heavyImpactIfEnabled()
            #endif
            return
        }

        if Self.isPromotion(last) {
            playPromoteSound()
            #if os(iOS)
            HapticSettings.mediumImpactIfEnabled()
            #endif
            return
        }

        if current.status.inCheck {
            playCheckSound()
            #if os(iOS)
            HapticSettings.heavyImpactIfEnabled()
            #endif
            return
        }

        playMoveSound()
    }

    // MARK: - Detekce typu tahu

    private static func isCastling(_ m: HistoryMove) -> Bool {
        if let san = m.san?.uppercased(), san.contains("O-O") { return true }
        let pk = m.piece
        guard pk == "K" || pk == "k" else { return false }
        guard let fromSq = m.from, let toSq = m.to,
              let f = ChessSquareNotation.indices(from: fromSq),
              let t = ChessSquareNotation.indices(from: toSq) else { return false }
        return abs(t.col - f.col) == 2
    }

    private static func isPromotion(_ m: HistoryMove) -> Bool {
        if let san = m.san, san.contains("=") { return true }
        guard let piece = m.piece, piece.lowercased() == "p" else { return false }
        guard let toSq = m.to, let to = ChessSquareNotation.indices(from: toSq) else { return false }
        return to.row == 0 || to.row == 7
    }

    // MARK: - Přehrání WAV z balíčku

    private func configureAudioSessionIfNeeded() {
        guard !sessionConfigured else { return }
        sessionConfigured = true
        #if os(iOS)
        let session = AVAudioSession.sharedInstance()
        do {
            try session.setCategory(.ambient, mode: .default, options: [.mixWithOthers])
            try session.setActive(true)
        } catch {
            #if DEBUG
            AppDebugLog.staging("SoundManager: AVAudioSession \(error.localizedDescription)")
            #endif
        }
        #endif
    }

    private func playBundled(_ resourceName: String, fallbackSystem: SystemSoundID) {
        guard isEnabled else { return }
        configureAudioSessionIfNeeded()
        guard let player = audioPlayer(for: resourceName) else {
            AudioServicesPlaySystemSound(fallbackSystem)
            return
        }
        player.currentTime = 0
        player.play()
    }

    private func audioPlayer(for resourceName: String) -> AVAudioPlayer? {
        if let existing = players[resourceName] { return existing }
        guard let url = Self.bundleURL(for: resourceName) else { return nil }
        do {
            let p = try AVAudioPlayer(contentsOf: url)
            p.prepareToPlay()
            players[resourceName] = p
            return p
        } catch {
            #if DEBUG
            AppDebugLog.staging("SoundManager: AVAudioPlayer \(resourceName) — \(error.localizedDescription)")
            #endif
            return nil
        }
    }

    private static func bundleURL(for resourceName: String) -> URL? {
        if let u = Bundle.main.url(forResource: resourceName, withExtension: "wav", subdirectory: "ChessSounds") {
            return u
        }
        return Bundle.main.url(forResource: resourceName, withExtension: "wav")
    }
}
