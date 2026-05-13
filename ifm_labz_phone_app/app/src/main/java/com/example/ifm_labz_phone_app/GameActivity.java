package com.example.ifm_labz_phone_app;

import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.os.Bundle;
import android.widget.TextView;
import androidx.appcompat.app.AppCompatActivity;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.net.InetAddress;
import java.net.NetworkInterface;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.Collections;
import java.util.List;
import java.util.Locale;

public class GameActivity extends AppCompatActivity {

    private TextView tvServerIp, tvStatus;
    private ServerSocket serverSocket;
    private Thread serverThread;

    private String gameDifficulty;

    private java.util.List<Socket> activeClients = new java.util.ArrayList<>();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_game);

        if (getIntent() != null && getIntent().hasExtra("SELECTED_DIFFICULTY")) {
            gameDifficulty = getIntent().getStringExtra("SELECTED_DIFFICULTY");
        } else {
            gameDifficulty = "MEDIUM";
        }

        tvServerIp = findViewById(R.id.tvServerIp);
        tvStatus = findViewById(R.id.tvStatus);

        String ip = getLocalIpAddress();
        tvServerIp.setText("SERVER IP: " + ip + " | PORT: 8080 | MODE: "+gameDifficulty);
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
        private Socket socket;
        public ClientHandler(Socket socket) { this.socket = socket; }

        @Override
        public void run() {
            try {
                socket.setTcpNoDelay(true);

                BufferedReader in = new BufferedReader(new InputStreamReader(socket.getInputStream()));
                PrintWriter out = new PrintWriter(socket.getOutputStream(), true);

                String message;
                while ((message = in.readLine()) != null) {
                    updateLog("RECEIVED: " + message);

                    if (message.contains("SUBMIT")) {
                        out.println("DATA-ANALYSED");
                        out.flush();
                    } else if (message.contains("BOOST")) {
                        out.println("BOOSTER ACTIVATED");
                        out.flush();
                    }
                }
            } catch (IOException e) {
                updateLog(">> A CONSOLE HAS DISCONNECTED");
                e.printStackTrace();
            }
        }
    }

    private void updateLog(final String message) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                tvStatus.append(message + "\n");
            }
        });
    }
    private String getLocalIpAddress() {
        try {
            List<NetworkInterface> interfaces = Collections.list(NetworkInterface.getNetworkInterfaces());
            for (NetworkInterface intf : interfaces) {
                List<InetAddress> addrs = Collections.list(intf.getInetAddresses());
                for (InetAddress addr : addrs) {
                    if (!addr.isLoopbackAddress()) {
                        String sAddr = addr.getHostAddress();
                        boolean isIPv4 = sAddr.indexOf(':') < 0;

                        if (isIPv4) {
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
        if (serverSocket != null) {
            try { serverSocket.close(); } catch (IOException e) { e.printStackTrace(); }
        }
        for (Socket client : activeClients) {
            if (client != null) {
                try { client.close(); } catch (IOException e) { e.printStackTrace(); }
            }
        }
    }
}