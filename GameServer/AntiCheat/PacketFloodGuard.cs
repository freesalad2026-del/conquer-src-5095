using System;
using System.Diagnostics;
using System.Runtime.CompilerServices;
using COServer.ServerSockets;

namespace COServer.AntiCheat
{
    /// <summary>
    /// Additional SERVER-SIDE protection. This does not inspect clients'
    /// computers, depend on the TQHandle DLL, or replace the TQGuard service.
    /// Counts packets separately for each established game socket.
    /// </summary>
    internal static class PacketFloodGuard
    {
        private sealed class Window
        {
            internal long StartedAt = Stopwatch.GetTimestamp();
            internal long LastLoggedAt;
            internal int Count;
            internal int ConsecutiveOverLimitWindows;
        }

        // A weak-key collection prevents stale sessions being retained after disconnect.
        private static readonly ConditionalWeakTable<SecuritySocket, Window> Windows =
            new ConditionalWeakTable<SecuritySocket, Window>();

        /// <summary>
        /// Returns false only for clearly excessive packet floods.
        /// The handshake is intentionally excluded by the call site.
        /// </summary>
        internal static bool Allow(SecuritySocket socket, ushort packetId)
        {
            if (!Program.ServerConfig.EnablePacketFloodGuard || socket == null)
                return true;

            int softLimit = (int)Math.Max(100u, Math.Min(5000u,
                Program.ServerConfig.PacketFloodSoftLimit));
            int hardLimit = (int)Math.Max((uint)(softLimit + 1), Math.Min(15000u,
                Program.ServerConfig.PacketFloodHardLimit));

            Window window = Windows.GetValue(socket, ignored => new Window());
            bool disconnect;
            bool log;
            int count;
            lock (window)
            {
                long now = Stopwatch.GetTimestamp();
                long elapsed = now - window.StartedAt;
                if (elapsed >= Stopwatch.Frequency)
                {
                    // Long-idle connections must not carry old infractions forward.
                    if (elapsed >= Stopwatch.Frequency * 2)
                        window.ConsecutiveOverLimitWindows = 0;
                    else if (window.Count > softLimit)
                        window.ConsecutiveOverLimitWindows++;
                    else
                        window.ConsecutiveOverLimitWindows = 0;

                    window.StartedAt = now;
                    window.Count = 0;
                }

                count = ++window.Count;
                // Hard spike: disconnect immediately.
                // Moderate spikes: disconnect only during a third consecutive second.
                disconnect = count > hardLimit ||
                             (count > softLimit &&
                              window.ConsecutiveOverLimitWindows >= 2);

                log = disconnect ||
                      (count == softLimit + 1 &&
                       now - window.LastLoggedAt >= Stopwatch.Frequency * 15);
                if (log)
                    window.LastLoggedAt = now;
            }

            if (log)
            {
                // No usernames, credentials or payload data are recorded.
                System.Console.WriteLine(
                    "[AntiCheat] Packet flood {0}: ip={1} packet={2} window_count={3} limit={4}",
                    disconnect ? "DISCONNECT" : "WARNING",
                    socket.RemoteIp ?? "unknown",
                    packetId, count, disconnect ? hardLimit : softLimit);
            }

            return !disconnect;
        }
    }
}
