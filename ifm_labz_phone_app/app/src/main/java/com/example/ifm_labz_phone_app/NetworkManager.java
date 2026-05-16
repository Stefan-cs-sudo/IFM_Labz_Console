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
            new Thread(() -> {
                try {
                    PrintWriter out = new PrintWriter(socket.getOutputStream(), true);
                    out.println(cmd);
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }).start();
        }
    }

    public void broadcast(String cmd) {
        sendToConsole(alphaSocket, cmd);
        if (betaSocket != null && betaSocket != alphaSocket) {
            sendToConsole(betaSocket, cmd);
        }
    }
}