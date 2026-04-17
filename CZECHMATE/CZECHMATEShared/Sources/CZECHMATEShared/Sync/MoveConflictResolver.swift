//
//  MoveConflictResolver.swift
//  CZECHMATEShared
//
//  iPhone priority conflict resolution for multi-device chess moves.
//

import Foundation
import Observation

/// Device types that can submit moves
public enum DeviceType: String, Sendable, Codable {
    case iPhone
    case watch
}

/// Represents a chess move with metadata
public struct ChessMove: Sendable, Codable {
    public let from: String
    public let to: String
    public let promotion: String?
    public let timestamp: Date
    
    public init(from: String, to: String, promotion: String? = nil, timestamp: Date = Date()) {
        self.from = from
        self.to = to
        self.promotion = promotion
        self.timestamp = timestamp
    }
}

/// Ticket for tracking a queued move
public struct MoveTicket: Identifiable, Sendable, Codable {
    public let id: UUID
    public let move: ChessMove
    public let device: DeviceType
    public var isValid: Bool
    public let createdAt: Date
    
    public init(move: ChessMove, device: DeviceType, isValid: Bool = true) {
        self.id = UUID()
        self.move = move
        self.device = device
        self.isValid = isValid
        self.createdAt = Date()
    }
    
    public mutating func invalidate() {
        isValid = false
    }
    
    enum CodingKeys: String, CodingKey {
        case id, move, device, isValid, createdAt
    }
}

/// Delegate for conflict resolution events
public protocol MoveConflictResolverDelegate: AnyObject {
    func moveTicket(_ ticket: MoveTicket, didChangeValidity isValid: Bool)
    func moveTicketDidExpire(_ ticket: MoveTicket)
}

/// Manages move queuing and conflict resolution with iPhone priority
@Observable
@MainActor
public final class MoveConflictResolver {
    /// iPhone priority window in seconds (500ms)
    public static let iPhonePriorityWindow: TimeInterval = 0.5
    
    /// Singleton instance
    public static let shared = MoveConflictResolver()
    
    /// Delegate for event callbacks
    public weak var delegate: MoveConflictResolverDelegate?
    
    /// Current pending moves
    private var pendingTickets: [MoveTicket] = []
    
    /// Last move execution time
    private var lastMoveTime: Date?
    
    /// Whether iPhone is currently processing a move (blocks Watch)
    private var isProcessingiPhoneMove: Bool = false
    
    private init() {}
    
    // MARK: - Public API
    
    /// Queue a move for execution with conflict checking
    /// - Parameters:
    ///   - move: The chess move to execute
    ///   - device: Device submitting the move
    /// - Returns: MoveTicket for tracking status
    public func queueMove(_ move: ChessMove, from device: DeviceType) -> MoveTicket {
        let ticket = MoveTicket(move: move, device: device)
        
        if device == .iPhone {
            // iPhone moves execute immediately
            isProcessingiPhoneMove = true
            lastMoveTime = Date()
            invalidateConflictingWatchMoves(move)
            
            // Reset processing flag after priority window
            DispatchQueue.main.asyncAfter(deadline: .now() + Self.iPhonePriorityWindow) { [weak self] in
                self?.isProcessingiPhoneMove = false
            }
        } else {
            // Watch moves need validation
            if shouldAllowWatchMove(ticket) {
                pendingTickets.append(ticket)
                scheduleValidation(for: ticket)
            } else {
                var invalidTicket = ticket
                invalidTicket.invalidate()
                return invalidTicket
            }
        }
        
        return ticket
    }
    
    /// Check if a move ticket is still valid for execution
    public func isValid(_ ticket: MoveTicket) -> Bool {
        if ticket.device == .iPhone {
            return true // iPhone moves are always valid
        }
        return ticket.isValid && !isProcessingiPhoneMove
    }
    
    /// Mark a move as executed (removes from pending)
    public func markExecuted(_ ticket: MoveTicket) {
        pendingTickets.removeAll { $0.id == ticket.id }
        lastMoveTime = Date()
    }
    
    /// Cancel a pending move
    public func cancelMove(_ ticket: MoveTicket) {
        pendingTickets.removeAll { $0.id == ticket.id }
        var cancelled = ticket
        cancelled.invalidate()
        delegate?.moveTicket(cancelled, didChangeValidity: false)
    }
    
    /// Reset resolver state (e.g., new game started)
    public func reset() {
        pendingTickets.removeAll()
        lastMoveTime = nil
        isProcessingiPhoneMove = false
    }
    
    // MARK: - Private Methods
    
    private func shouldAllowWatchMove(_ ticket: MoveTicket) -> Bool {
        // Block Watch moves during iPhone priority window
        if isProcessingiPhoneMove {
            return false
        }
        
        // Check if we just had an iPhone move
        if let lastTime = lastMoveTime,
           Date().timeIntervalSince(lastTime) < Self.iPhonePriorityWindow {
            return false
        }
        
        return true
    }
    
    private func invalidateConflictingWatchMoves(_ iPhoneMove: ChessMove) {
        // Invalidate any pending Watch moves that conflict
        for i in pendingTickets.indices where pendingTickets[i].device == .watch {
            // Check if move is still valid after iPhone move
            // This is a simplified check - in reality we'd validate against new board state
            pendingTickets[i].invalidate()
            delegate?.moveTicket(pendingTickets[i], didChangeValidity: false)
        }
        
        // Remove invalidated tickets
        pendingTickets.removeAll { !$0.isValid }
    }
    
    private func scheduleValidation(for ticket: MoveTicket) {
        // Validate Watch move after iPhone priority window
        DispatchQueue.main.asyncAfter(deadline: .now() + Self.iPhonePriorityWindow) { [weak self] in
            guard let self = self else { return }
            
            if let index = self.pendingTickets.firstIndex(where: { $0.id == ticket.id }) {
                let currentTicket = self.pendingTickets[index]
                if !currentTicket.isValid || self.isProcessingiPhoneMove {
                    self.delegate?.moveTicket(currentTicket, didChangeValidity: false)
                    self.pendingTickets.remove(at: index)
                }
            }
        }
    }
}

// MARK: - Convenience Extensions

public extension MoveConflictResolver {
    /// Quick check if Watch can execute move now
    var canWatchMoveExecute: Bool {
        !isProcessingiPhoneMove
    }
    
    /// Time remaining in iPhone priority window
    var iPhonePriorityWindowRemaining: TimeInterval {
        guard let lastTime = lastMoveTime else { return 0 }
        let elapsed = Date().timeIntervalSince(lastTime)
        return max(0, Self.iPhonePriorityWindow - elapsed)
    }
}
