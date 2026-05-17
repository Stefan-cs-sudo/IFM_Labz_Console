package com.example.ifm_labz_phone_app;

import java.io.PrintWriter;
import java.net.Socket;

public class NetworkManager {
    private static NetworkManager instance;

    public Socket alphaSocket = null;
    public Socket betaSocket = null;

    private NetworkManager() {}

    public static synchronized NetworkManager getInstance() {
        if (instance == null) {
            instance = new NetworkManager();
        }
        return instance;
    }

    public void sendToConsole(Socket socket, String cmd) {
        if (socket != null && !socket.isClosed()) {
            try {
                PrintWriter out = new PrintWriter(socket.getOutputStream(), true);
                out.println(cmd);
                out.flush();
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }

    public void broadcast(String cmd) {
        sendToConsole(alphaSocket, cmd);
        if (betaSocket != null && betaSocket != alphaSocket) {
            sendToConsole(betaSocket, cmd);
        }
    }

    public void closeAll() {
        try {
            if (alphaSocket != null && !alphaSocket.isClosed()) alphaSocket.close();
        } catch (Exception ignored) {}
        try {
            if (betaSocket != null && !betaSocket.isClosed()) betaSocket.close();
        } catch (Exception ignored) {}

        alphaSocket = null;
        betaSocket = null;
    }
}