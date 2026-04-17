//
//  RemoteChessMoveLegality.swift
//  CZECHMATE — lokální legální tah před POST /api/move nebo BLE move (parita s `snapshot.board`, řádek 0 = rank 8 jako ve firmware).
//

import Foundation

/// Důvod, proč tah neposíláme na desku — žádný HTTP, žádný „fyzický“ error recovery na MCU.
enum RemoteMoveBlockReason: Equatable, Sendable {
    case noSnapshot
    case gameFinished
    case webLocked
    case boardErrorRecoveryActive
    case castlingInProgress
    case invalidSquareNotation
    case noPieceAtFrom
    case notYourPiece
    case ownPieceOnDestination
    case promotionMissing
    case promotionInvalid
    case illegalByRules

    var userMessage: String {
        switch self {
        case .noSnapshot:
            return "Chybí aktuální stav šachovnice. Počkej na synchronizaci se deskou."
        case .gameFinished:
            return "Partie už skončila — nelze táhnout z aplikace."
        case .webLocked:
            return "Webové rozhraní na desce je zamčené (odemkni přes UART)."
        case .boardErrorRecoveryActive:
            return "Na desce probíhá oprava neplatného tahu. Nejdřív to vyřeš fyzicky podle LED, pak zkus znovu."
        case .castlingInProgress:
            return "Na desce probíhá rošáda — dokonči ji na šachovnici, pak táhni z aplikace."
        case .invalidSquareNotation:
            return "Neplatné označení pole."
        case .noPieceAtFrom:
            return "Na výchozím poli není figurka (zkontroluj synchronizaci s deskou)."
        case .notYourPiece:
            return "Nejsi na tahu nebo taháš cizí figurku."
        case .ownPieceOnDestination:
            return "Cílové pole obsahuje tvoji figurku."
        case .promotionMissing:
            return "Vyber figuru pro promoci."
        case .promotionInvalid:
            return "Neplatná promoci (použij dámu, věž, střelce nebo jezdce)."
        case .illegalByRules:
            return "Tento tah není v této pozici legální."
        }
    }
}

enum RemoteChessMoveLegality {
    /// Figurka na poli (první znak buňky), `nil` mimo desku / prázdné.
    static func pieceChar(board: [[String]], square: String) -> Character? {
        guard let idx = FirmwareSquareNotation.indices(from: square) else { return nil }
        guard idx.row >= 0, idx.row < 8, idx.col >= 0, idx.col < 8,
              idx.row < board.count, idx.col < board[idx.row].count else { return nil }
        let s = board[idx.row][idx.col].trimmingCharacters(in: .whitespaces)
        guard let ch = s.first, ch != " " else { return nil }
        return ch
    }

    /// Strana na tahu podle `status.current_player` (ESP může poslat „White“ / „white“).
    static func sideToMoveIsWhite(currentPlayer: String) -> Bool {
        let t = currentPlayer.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
        if t == "black" || t == "b" { return false }
        if t == "white" || t == "w" { return true }
        if currentPlayer == "Black" { return false }
        if currentPlayer == "White" { return true }
        return true
    }

    static func isOwnPiece(_ piece: Character, currentPlayer: String) -> Bool {
        let pieceIsWhite = piece.isUppercase && piece.isLetter
        return sideToMoveIsWhite(currentPlayer: currentPlayer)
            ? pieceIsWhite
            : (piece.isLetter && !pieceIsWhite)
    }

    /// Zda je potřeba nejdřív z UI zvolit promoci (`q/r/n/b`) před odesláním tahu.
    /// Platí jen pro **pěšce** na cílové promoční řadě vlastní barvy (řádek 0 = rank 8, 7 = rank 1).
    static func moveNeedsPromotionPrompt(board: [[String]], from: String, to: String) -> Bool {
        guard let f = FirmwareSquareNotation.indices(from: from),
              let t = FirmwareSquareNotation.indices(from: to),
              board.count == 8 else { return false }
        let pieceStr = board[f.row][f.col].trimmingCharacters(in: .whitespaces)
        guard let pieceCh = pieceStr.first, pieceCh != " " else { return false }
        let lower = Character(String(pieceCh).lowercased())
        guard lower == "p" else { return false }
        let pieceIsWhite = pieceCh.isUppercase && pieceCh.isLetter
        let promoRow = pieceIsWhite ? 0 : 7
        return t.row == promoRow
    }

    /// Vrátí `nil`, pokud je tah v pořádku a může se odeslat na desku.
    static func validate(snapshot: GameSnapshot, from: String, to: String, promotion: String?) -> RemoteMoveBlockReason? {
        if snapshot.status.isGameFinished { return .gameFinished }
        if snapshot.status.webLocked == true { return .webLocked }
        if snapshot.status.isErrorRecoveryActive { return .boardErrorRecoveryActive }
        if snapshot.status.castlingInProgress == true { return .castlingInProgress }
        guard snapshot.board.count == 8 else { return .noSnapshot }

        guard let f = FirmwareSquareNotation.indices(from: from),
              let t = FirmwareSquareNotation.indices(from: to) else { return .invalidSquareNotation }

        let pieceStr = snapshot.board[f.row][f.col].trimmingCharacters(in: .whitespaces)
        guard let pieceCh = pieceStr.first, pieceCh != " " else { return .noPieceAtFrom }

        let sideWhite = sideToMoveIsWhite(currentPlayer: snapshot.status.currentPlayer)
        guard isOwnPiece(pieceCh, currentPlayer: snapshot.status.currentPlayer) else { return .notYourPiece }

        let destStr = snapshot.board[t.row][t.col].trimmingCharacters(in: .whitespaces)
        if let destCh = destStr.first, destCh != " " {
            let destWhite = destCh.isUppercase && destCh.isLetter
            if sideWhite == destWhite { return .ownPieceOnDestination }
        }

        let lower = Character(String(pieceCh).lowercased())
        let promoRank = sideWhite ? 0 : 7
        let needsPromo = lower == "p" && t.row == promoRank
        if needsPromo {
            guard let p = promotion?.trimmingCharacters(in: .whitespacesAndNewlines).lowercased(), !p.isEmpty else {
                return .promotionMissing
            }
            guard "qrnb".contains(p) else { return .promotionInvalid }
        } else if promotion != nil, !promotion!.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
            // Nepožadovaná promoci v payloadu ignorujeme (MCU stejně); lokálně neblokujeme.
        }

        let ep = enPassantTarget(fromHistory: snapshot.history.moves, board: snapshot.board)
        let promoChar: Character? = {
            guard needsPromo, let p = promotion?.lowercased().first else { return nil }
            return sideWhite ? Character(String(p).uppercased()) : p
        }()

        if !isLegalMove(
            board: snapshot.board,
            sideWhite: sideWhite,
            from: f,
            to: t,
            promotion: promoChar,
            enPassantTarget: ep
        ) {
            return .illegalByRules
        }

        return nil
    }

    // MARK: - En passant z posledního tahu (jako FEN ep square)

    private static func enPassantTarget(fromHistory moves: [GameHistoryMove], board: [[String]]) -> (row: Int, col: Int)? {
        guard let last = moves.last,
              let fs = last.from, let ts = last.to,
              let f = FirmwareSquareNotation.indices(from: fs),
              let t = FirmwareSquareNotation.indices(from: ts) else { return nil }
        // Po tahu je pěšec na cílovém poli; výchozí pole může být prázdné.
        let pawnNow = cell(board, t.row, t.col)
        guard String(pawnNow).lowercased() == "p" else { return nil }
        guard f.col == t.col, abs(f.row - t.row) == 2 else { return nil }
        let mid = (f.row + t.row) / 2
        return (mid, t.col)
    }

    // MARK: - Legální tah (pseudo + král není v šachu)

    private static func isLegalMove(
        board: [[String]],
        sideWhite: Bool,
        from: (row: Int, col: Int),
        to: (row: Int, col: Int),
        promotion: Character?,
        enPassantTarget: (row: Int, col: Int)?
    ) -> Bool {
        guard var b = cloneBoard(board) else { return false }
        if !isPseudoLegal(board: &b, sideWhite: sideWhite, from: from, to: to, promotion: promotion, enPassantTarget: enPassantTarget) {
            return false
        }
        return !inCheck(board: b, whiteKing: sideWhite)
    }

    private static func cloneBoard(_ board: [[String]]) -> [[String]]? {
        guard board.count == 8 else { return nil }
        var out: [[String]] = []
        for r in 0 ..< 8 {
            guard board[r].count == 8 else { return nil }
            out.append(board[r].map { String($0.trimmingCharacters(in: .whitespaces).first ?? " ") })
        }
        return out
    }

    private static func cell(_ board: [[String]], _ r: Int, _ c: Int) -> Character {
        guard r >= 0, r < 8, c >= 0, c < 8 else { return " " }
        return board[r][c].first ?? " "
    }

    private static func isPseudoLegal(
        board b: inout [[String]],
        sideWhite: Bool,
        from: (row: Int, col: Int),
        to: (row: Int, col: Int),
        promotion: Character?,
        enPassantTarget: (row: Int, col: Int)?
    ) -> Bool {
        let p = cell(b, from.row, from.col)
        guard p != " " else { return false }
        let lower = String(p).lowercased().first!

        if lower == "k" {
            return applyKingMove(&b, sideWhite: sideWhite, from: from, to: to)
        }

        if !movePatternMatches(board: b, sideWhite: sideWhite, piece: lower, from: from, to: to, enPassantTarget: enPassantTarget) {
            return false
        }

        // en passant: odstranit pěšce za cílem
        if lower == "p", let ep = enPassantTarget, to.row == ep.row, to.col == ep.col {
            let capRow = sideWhite ? to.row + 1 : to.row - 1
            guard capRow >= 0, capRow < 8 else { return false }
            let cap = cell(b, capRow, to.col)
            let capIsWhite = cap.isUppercase && cap.isLetter
            guard String(cap).lowercased() == "p", capIsWhite != sideWhite else { return false }
            b[capRow][to.col] = " "
        }

        b[to.row][to.col] = {
            if lower == "p", let pr = promotion {
                return String(pr)
            }
            return String(p)
        }()
        b[from.row][from.col] = " "
        return true
    }

    private static func movePatternMatches(
        board: [[String]],
        sideWhite: Bool,
        piece: Character,
        from: (row: Int, col: Int),
        to: (row: Int, col: Int),
        enPassantTarget: (row: Int, col: Int)?
    ) -> Bool {
        let dr = to.row - from.row
        let dc = to.col - from.col
        let dest = cell(board, to.row, to.col)
        let destOccupied = dest != " "

        switch piece {
        case "p":
            let forward = sideWhite ? -1 : 1
            let startRank = sideWhite ? 6 : 1
            if dc == 0, !destOccupied {
                if dr == forward { return true }
                if from.row == startRank, dr == 2 * forward {
                    let mid = cell(board, from.row + forward, from.col)
                    return mid == " "
                }
                return false
            }
            if abs(dc) == 1, dr == forward {
                if destOccupied { return true }
                if let ep = enPassantTarget, ep.row == to.row, ep.col == to.col { return true }
            }
            return false
        case "n":
            let adR = abs(dr), adC = abs(dc)
            return (adR == 2 && adC == 1) || (adR == 1 && adC == 2)
        case "b":
            return slidingAlong(board: board, from: from, to: to, dr: dr, dc: dc, diagonal: true)
        case "r":
            return slidingAlong(board: board, from: from, to: to, dr: dr, dc: dc, diagonal: false)
        case "q":
            return slidingAlong(board: board, from: from, to: to, dr: dr, dc: dc, diagonal: true)
            || slidingAlong(board: board, from: from, to: to, dr: dr, dc: dc, diagonal: false)
        default:
            return false
        }
    }

    private static func slidingAlong(
        board: [[String]],
        from: (row: Int, col: Int),
        to: (row: Int, col: Int),
        dr: Int,
        dc: Int,
        diagonal: Bool
    ) -> Bool {
        if dr == 0, dc == 0 { return false }
        let stepR = dr == 0 ? 0 : (dr > 0 ? 1 : -1)
        let stepC = dc == 0 ? 0 : (dc > 0 ? 1 : -1)
        if diagonal {
            guard abs(dr) == abs(dc) else { return false }
        } else {
            guard dr == 0 || dc == 0 else { return false }
        }
        var r = from.row + stepR
        var c = from.col + stepC
        while r != to.row || c != to.col {
            guard r >= 0, r < 8, c >= 0, c < 8 else { return false }
            if cell(board, r, c) != " " { return false }
            r += stepR
            c += stepC
        }
        return true
    }

    private static func applyKingMove(
        _ board: inout [[String]],
        sideWhite: Bool,
        from: (row: Int, col: Int),
        to: (row: Int, col: Int)
    ) -> Bool {
        let dr = to.row - from.row
        let dc = to.col - from.col
        let p = cell(board, from.row, from.col)

        if max(abs(dr), abs(dc)) == 1 {
            board[to.row][to.col] = String(p)
            board[from.row][from.col] = " "
            return true
        }

        // Rošáda (král o 2 pole)
        guard dr == 0, abs(dc) == 2 else { return false }
        let row = from.row
        guard !inCheck(board: board, whiteKing: sideWhite) else { return false }

        if sideWhite, row == 7, from.col == 4 {
            if dc == 2 {
                guard cell(board, 7, 5) == " ", cell(board, 7, 6) == " " else { return false }
                guard String(cell(board, 7, 7)) == "R" else { return false }
                guard !isSquareAttacked(board: board, row: 7, col: 5, byWhite: false),
                      !isSquareAttacked(board: board, row: 7, col: 6, byWhite: false) else { return false }
                board[7][4] = " "
                board[7][6] = "K"
                board[7][7] = " "
                board[7][5] = "R"
                return true
            }
            if dc == -2 {
                guard cell(board, 7, 1) == " ", cell(board, 7, 2) == " ", cell(board, 7, 3) == " " else { return false }
                guard String(cell(board, 7, 0)) == "R" else { return false }
                guard !isSquareAttacked(board: board, row: 7, col: 3, byWhite: false),
                      !isSquareAttacked(board: board, row: 7, col: 2, byWhite: false) else { return false }
                board[7][4] = " "
                board[7][2] = "K"
                board[7][0] = " "
                board[7][3] = "R"
                return true
            }
        }

        if !sideWhite, row == 0, from.col == 4 {
            if dc == 2 {
                guard cell(board, 0, 5) == " ", cell(board, 0, 6) == " " else { return false }
                guard cell(board, 0, 7) == "r" else { return false }
                guard !isSquareAttacked(board: board, row: 0, col: 5, byWhite: true),
                      !isSquareAttacked(board: board, row: 0, col: 6, byWhite: true) else { return false }
                board[0][4] = " "
                board[0][6] = "k"
                board[0][7] = " "
                board[0][5] = "r"
                return true
            }
            if dc == -2 {
                guard cell(board, 0, 1) == " ", cell(board, 0, 2) == " ", cell(board, 0, 3) == " " else { return false }
                guard cell(board, 0, 0) == "r" else { return false }
                guard !isSquareAttacked(board: board, row: 0, col: 3, byWhite: true),
                      !isSquareAttacked(board: board, row: 0, col: 2, byWhite: true) else { return false }
                board[0][4] = " "
                board[0][2] = "k"
                board[0][0] = " "
                board[0][3] = "r"
                return true
            }
        }

        return false
    }

    private static func inCheck(board: [[String]], whiteKing: Bool) -> Bool {
        let target: Character = whiteKing ? "K" : "k"
        for r in 0 ..< 8 {
            for c in 0 ..< 8 {
                if cell(board, r, c) == target {
                    return isSquareAttacked(board: board, row: r, col: c, byWhite: !whiteKing)
                }
            }
        }
        return false
    }

    /// Útok bílými (`byWhite == true`) nebo černými figurami na pole (r,c).
    private static func isSquareAttacked(board: [[String]], row: Int, col: Int, byWhite: Bool) -> Bool {
        for r in 0 ..< 8 {
            for c in 0 ..< 8 {
                let p = cell(board, r, c)
                guard p != " " else { continue }
                let isW = p.isUppercase && p.isLetter
                if isW != byWhite { continue }
                let lower = String(p).lowercased().first!
                if pieceAttacks(board: board, pieceLower: lower, pieceWhite: isW, fromRow: r, fromCol: c, toRow: row, toCol: col) {
                    return true
                }
            }
        }
        return false
    }

    private static func pieceAttacks(
        board: [[String]],
        pieceLower: Character,
        pieceWhite: Bool,
        fromRow: Int,
        fromCol: Int,
        toRow: Int,
        toCol: Int
    ) -> Bool {
        let dr = toRow - fromRow
        let dc = toCol - fromCol
        switch pieceLower {
        case "n":
            let adR = abs(dr), adC = abs(dc)
            return (adR == 2 && adC == 1) || (adR == 1 && adC == 2)
        case "k":
            return max(abs(dr), abs(dc)) == 1
        case "r":
            return slidingAlong(board: board, from: (fromRow, fromCol), to: (toRow, toCol), dr: dr, dc: dc, diagonal: false)
        case "b":
            return slidingAlong(board: board, from: (fromRow, fromCol), to: (toRow, toCol), dr: dr, dc: dc, diagonal: true)
        case "q":
            return slidingAlong(board: board, from: (fromRow, fromCol), to: (toRow, toCol), dr: dr, dc: dc, diagonal: true)
                || slidingAlong(board: board, from: (fromRow, fromCol), to: (toRow, toCol), dr: dr, dc: dc, diagonal: false)
        case "p":
            let forward = pieceWhite ? -1 : 1
            return abs(dc) == 1 && dr == forward
        default:
            return false
        }
    }
}
