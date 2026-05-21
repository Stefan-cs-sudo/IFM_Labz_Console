package com.example.ifm_labz_phone_app;

import android.content.Intent;
import android.os.Bundle;
import android.widget.TextView;
import androidx.appcompat.app.AppCompatActivity;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.net.InetAddress;
import java.net.NetworkInterface;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

public class LobbyActivity extends AppCompatActivity {

    private TextView tvServerIp, tvStatus;
    private ServerSocket serverSocket;
    private Thread serverThread;

    private String gameDifficulty = "MEDIUM";
    private String gameMode = "SINGLE";
    private final AtomicBoolean transitioning = new AtomicBoolean(false);

    private final java.util.List<Socket> activeClients = new java.util.ArrayList<>();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_game);

        if (getIntent() != null && getIntent().hasExtra("GAME_MODE")) {
            gameMode = getIntent().getStringExtra("GAME_MODE");
        }

        if (getIntent() != null && getIntent().hasExtra("SELECTED_DIFFICULTY")) {
            gameDifficulty = getIntent().getStringExtra("SELECTED_DIFFICULTY");
        }

        tvServerIp = findViewById(R.id.tvServerIp);
        tvStatus = findViewById(R.id.tvStatus);

        String ip = getLocalIpAddress();
        tvServerIp.setText("SERVER IP: " + ip + " | PORT: 8080 | MODE: " + gameDifficulty);
        updateLog("Waiting for the consoles to connect...");

        serverThread = new Thread(new ServerThread());
        serverThread.start();
    }

    class ServerThread implements Runnable {
        @Override
        public void run() {
            try {
                serverSocket = new ServerSocket(8080);
                while (!Thread.currentThread().isInterrupted()) {
                    Socket clientSocket = serverSocket.accept();
                    activeClients.add(clientSocket);
                    updateLog(">> A CONSOLE HAS CONNECTED: " + clientSocket.getInetAddress().getHostAddress());

                    new Thread(new ClientHandler(clientSocket)).start();
                }
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
    }

    class ClientHandler implements Runnable {
        private final Socket socket;
        public ClientHandler(Socket socket) { this.socket = socket; }

        @Override
        public void run() {
            try {
                socket.setTcpNoDelay(true);
                BufferedReader in = new BufferedReader(new InputStreamReader(socket.getInputStream()));
                String message;

                NetworkManager netManager = NetworkManager.getInstance();

                while (!transitioning.get() && (message = in.readLine()) != null) {
                    message = message.trim();
                    updateLog("RECEIVED: " + message);

                    updateLog("RAW: " + message);

                    if (message.contains("ALPHA|CONNECT")) {
                        netManager.alphaSocket = socket;
                        updateLog(">>> ALPHA CONNECTED");
                    } else if (message.contains("BETA|CONNECT")) {
                        netManager.betaSocket = socket;
                        updateLog(">>> BETA CONNECTED");
                    }


                    if (gameMode.equals("SINGLE") && netManager.alphaSocket != null) {
                        if (transitioning.compareAndSet(false, true)) {
                            netManager.betaSocket = netManager.alphaSocket;
                            shutdownServer();

                            runOnUiThread(() -> {
                                updateLog(">>> SINGLE MODE: START GAME");
                                Intent intent = new Intent(LobbyActivity.this, GameplayActivity.class);
                                intent.putExtra("SELECTED_DIFFICULTY", gameDifficulty);
                                intent.putExtra("GAME_MODE", gameMode);
                                startActivity(intent);
                                finish();
                            });
                        }
                        break;
                    }

                    // CO-OP
                    if (gameMode.equals("COOP") && netManager.alphaSocket != null && netManager.betaSocket != null) {
                        if (transitioning.compareAndSet(false, true)) {
                            shutdownServer();

                            runOnUiThread(() -> {
                                updateLog(">>> COOP MODE: START GAME");
                                Intent intent = new Intent(LobbyActivity.this, GameplayActivity.class);
                                intent.putExtra("SELECTED_DIFFICULTY", gameDifficulty);
                                intent.putExtra("GAME_MODE", gameMode);
                                startActivity(intent);
                                finish();
                            });
                        }
                        break;
                    }
                }
            } catch (IOException e) {
                updateLog(">> A CONSOLE HAS DISCONNECTED (Signal Lost)");
                e.printStackTrace();
            } finally {
                try {
                    if (transitioning.get()==false && socket != null && !socket.isClosed()) {
                        socket.close();
                    }
                    activeClients.remove(socket);
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }
    }


    private void shutdownServer() {
        try {
            if (serverSocket != null && !serverSocket.isClosed()) {
                serverSocket.close();
            }
        } catch (IOException ignored) {}

        if (serverThread != null) {
            serverThread.interrupt();
        }
    }

    private void updateLog(final String message) {
        runOnUiThread(() -> tvStatus.append(message + "\n"));
    }

    private String getLocalIpAddress() {
        try {
            List<NetworkInterface> interfaces = Collections.list(NetworkInterface.getNetworkInterfaces());
            for (NetworkInterface intf : interfaces) {
                List<InetAddress> addrs = Collections.list(intf.getInetAddresses());
                for (InetAddress addr : addrs) {
                    if (!addr.isLoopbackAddress()) {
                        String sAddr = addr.getHostAddress();
                        if (sAddr.indexOf(':') < 0) {
                            if (sAddr.startsWith("192.168.") || sAddr.startsWith("10.") || sAddr.startsWith("172.")) {
                                return sAddr;
                            }
                        }
                    }
                }
            }
        } catch (Exception ex) {
            ex.printStackTrace();
        }
        return "192.168.43.1";
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (serverThread != null) {
            serverThread.interrupt();
        }
        if (serverSocket != null && !serverSocket.isClosed()) {
            try { serverSocket.close(); } catch (IOException e) { e.printStackTrace(); }
        }

        if (transitioning.get()==false) {
            for (Socket client : activeClients) {
                if (client != null && !client.isClosed()) {
                    try { client.close(); } catch (IOException e) { e.printStackTrace(); }
                }
            }
            NetworkManager.getInstance().closeAll();
        }
    }
}