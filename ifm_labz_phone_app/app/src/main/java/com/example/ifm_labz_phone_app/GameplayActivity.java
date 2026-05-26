package com.example.ifm_labz_phone_app;

import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.widget.Button;
import android.widget.TextView;
import androidx.appcompat.app.AppCompatActivity;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.net.Socket;
import java.util.Random;

public class GameplayActivity extends AppCompatActivity {

    private TextView tvSector, tvTimer, tvIntegrity, tvTarget, tvStatusInfo;

    private NetworkManager netManager;

    private String gameDifficulty = "MEDIUM";
    private String gameMode = "SINGLE";

    private int currentSector = 1;
    private int systemIntegrity = 100;
    private int timeLeft = 75;
    private int nexusTarget = 0;


    private int alphaValue = 0;
    private int betaValue = 0;

    private final Handler uiHandler = new Handler(Looper.getMainLooper());
    private Runnable clockRunnable;
    private Runnable oscillationRunnable;
    private final Random random = new Random();

    private boolean b1Used = false, b2Used = false, b3Used = false, b4Used = false;
    private boolean isTargetFrozen = false;
    private boolean isTimeFrozen  =false;
    private boolean isGameOver = false;
    private Runnable revealTargetRunnable = null;
    private boolean isTargetRevealedTemporarily = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_gameplay);

        if (getIntent() != null && getIntent().hasExtra("SELECTED_DIFFICULTY")) {
            gameDifficulty = getIntent().getStringExtra("SELECTED_DIFFICULTY");
        }
        if (getIntent() != null && getIntent().hasExtra("GAME_MODE")) {
            gameMode = getIntent().getStringExtra("GAME_MODE");
        }

        netManager = NetworkManager.getInstance();

        tvSector = findViewById(R.id.tvSector);
        tvTimer = findViewById(R.id.tvTimer);
        tvIntegrity = findViewById(R.id.tvIntegrity);
        tvTarget = findViewById(R.id.tvTarget);
        tvStatusInfo = findViewById(R.id.tvStatusInfo);

        startListeningToConsoles();
        uiHandler.postDelayed(this::startNextSector, 3000);
    }

    private void startListeningToConsoles() {
        if (netManager.alphaSocket != null) {
            new Thread(() -> listenLoop(netManager.alphaSocket, "ALPHA")).start();
        }
        if (gameMode.equals("COOP") && netManager.betaSocket != null && netManager.betaSocket != netManager.alphaSocket) {
            new Thread(() -> listenLoop(netManager.betaSocket, "BETA")).start();
        }
    }

    private void listenLoop(Socket socket, String consoleName) {
        try {
            BufferedReader in = new BufferedReader(new InputStreamReader(socket.getInputStream()));
            String message;
            while (!isGameOver && (message = in.readLine()) != null) {
                final String finalMsg = message.trim();
                uiHandler.post(() -> processConsoleInput(consoleName, finalMsg));
            }
        } catch (IOException e) {
            uiHandler.post(() -> tvStatusInfo.setText(consoleName + " DECONECTED WHILE IN GAME!"));
        }
    }

    private void processConsoleInput(String consoleName, String message) {
        if (isGameOver) return;

        if (message.contains("SUBMIT:")) {
            try {
                int value = Integer.parseInt(message.split("SUBMIT:")[1]);

                if (consoleName.equals("ALPHA")) {
                    alphaValue = value;
                } else if (consoleName.equals("BETA")) {
                    if (gameMode.equals("COOP")) {
                        betaValue = value;
                    } else {
                        betaValue = 0;
                    }
                }

                verifyFormula(consoleName, value);
            } catch (Exception e) {
                tvStatusInfo.setText("Error: can't parse the frequency.");
            }
        } else if (message.contains("BOOST:")) {
            handleBooster(message);
        }
    }

    private String getTargetIntervalText() {
        int margin;
        if (gameDifficulty.equals("EASY")) margin = 20;
        else if (gameDifficulty.equals("MEDIUM")) margin = 50;
        else margin = 100;

        int lowRaw = nexusTarget - margin;
        int highRaw = nexusTarget + margin;

        int low = (int) Math.floor(lowRaw / 10.0) * 10;
        int high = (int) Math.ceil(highRaw / 10.0) * 10;

        if (low < 0) low = 0;
        if (high > 999) high = 999;

        return "NEXUS TARGET: [" + low + " - " + high + "]";
    }

    private void verifyFormula(String consoleName, int valueJustSent) {
        int currentSum = alphaValue + betaValue;


        if (valueJustSent == nexusTarget) {
            int other = consoleName.equals("ALPHA") ? betaValue : alphaValue;
            if (other == 0) {
                tvStatusInfo.setText("LUCKY MATCH DETECTED!");
                netManager.broadcast("CMD|LUCKY_ALARM");
                sectorCleared();
                return;
            }
        }

        if (currentSum == nexusTarget) {
            sectorCleared();
        } else {
            if (currentSum < nexusTarget) {
                netManager.broadcast("TOO LOW");
                tvStatusInfo.setText("Sum TOO LOW!");
            } else {
                netManager.broadcast("TOO HIGH|");
                tvStatusInfo.setText("Sum TOO HIGH!");
            }

            applyPenalty();
        }
    }

    private void applyPenalty() {
        systemIntegrity -= 5;
        tvIntegrity.setText("System Integrity: " + systemIntegrity + "%");
        if (systemIntegrity <= 0) {
            endGame(false, "INTEGRITY COMPROMISED (0%)");
        }
    }

    private void sectorCleared() {
        stopTimers();
        netManager.broadcast("MATCH");
        tvStatusInfo.setText("SECTOR " + currentSector + " UNLOCKED!");

        if (currentSector >= 3) {
            endGame(true, "VICTORY! ALL SECTORS HAVE BEEN BROKEN!");
        } else {
            currentSector++;
            uiHandler.postDelayed(this::startNextSector, 3000);
        }
    }

    private void startNextSector() {
        if (isGameOver) return;

        if (gameDifficulty.equals("EASY")) timeLeft = 90;
        else if (gameDifficulty.equals("MEDIUM")) timeLeft = 75;
        else timeLeft = 75;

        nexusTarget = random.nextInt(1000);

        alphaValue = 0;
        betaValue = 0;

        tvSector.setText("SECTOR: " + currentSector + " / 3");
        if (!isTargetRevealedTemporarily) {
            tvTarget.setText(getTargetIntervalText());
        }
        tvTimer.setText("TIME: " + timeLeft + "s");
        tvStatusInfo.setText("Sector initialized. Match the frequencies!");

        netManager.broadcast("CMD|START_GAME");

        startClock();
        startOscillationEngine();
    }

    private void startOscillationEngine() {
        if (gameDifficulty.equals("EASY")) return;

        int interval = gameDifficulty.equals("MEDIUM") ? 20000 : 10000;

        oscillationRunnable = new Runnable() {
            @Override
            public void run() {
                if (!isTargetFrozen && !isGameOver) {
                    int variation = gameDifficulty.equals("MEDIUM")
                            ? (random.nextInt(3) + 1)
                            : (random.nextInt(5) + 3);

                    if (random.nextBoolean()) variation = -variation;

                    nexusTarget += variation;

                    if (!isTargetRevealedTemporarily) {
                        tvTarget.setText(getTargetIntervalText() + " (OSCILLATION)");
                    }
                }
                uiHandler.postDelayed(this, interval);
            }
        };
        uiHandler.postDelayed(oscillationRunnable, interval);
    }

    private void startClock() {
        clockRunnable = new Runnable() {
            @Override
            public void run() {

                if (isGameOver) return;


                if (!isTimeFrozen) {
                    timeLeft--;
                    tvTimer.setText("TIME: " + timeLeft + "s");

                    if (timeLeft <= 0) {
                        endGame(false, "TIMEOUT: TIME HAS EXPIRED!");
                        return;
                    }
                }


                uiHandler.postDelayed(this, 1000);
            }
        };
        uiHandler.postDelayed(clockRunnable, 1000);
    }

    private void handleBooster(String boostMsg) {
        if (boostMsg.contains("BOOST:1") && !b1Used) {
            b1Used = true;
            timeLeft += 20;
            tvStatusInfo.setText("BOOSTER: Overclock activated! (+20s)");
        } else if (boostMsg.contains("BOOST:2") && !b2Used) {
            b2Used = true;
            isTargetFrozen = true;
            isTimeFrozen = true; // <-- FREEZE THE TIME

            tvStatusInfo.setText("BOOSTER: Cold Reboot! Time & Target frozen (15s)");

            uiHandler.postDelayed(() -> {
                isTargetFrozen = false;
                isTimeFrozen = false; // <-- UNFREEZE THE TIME
            }, 15000);
        } else if (boostMsg.contains("BOOST:3") && !b3Used) {
            b3Used = true;
            systemIntegrity = Math.min(100, systemIntegrity + 30);
            tvIntegrity.setText("System Integrity: " + systemIntegrity + "%");
            tvStatusInfo.setText("BOOSTER: Firewall Patch! (+30% Integrity)");
        } else if (boostMsg.contains("BOOST:4") && !b4Used) {
            b4Used = true;
            tvStatusInfo.setText("BOOSTER: Signal Filter ACTIVATED!");

            isTargetRevealedTemporarily = true;

            if (revealTargetRunnable != null) {
                uiHandler.removeCallbacks(revealTargetRunnable);
            }

            tvTarget.setText("NEXUS TARGET = " + nexusTarget);

            revealTargetRunnable = () -> {
                isTargetRevealedTemporarily = false;
                tvTarget.setText(getTargetIntervalText());
            };
            uiHandler.postDelayed(revealTargetRunnable, 2500);
        }
    }

    private void endGame(boolean success, String reason) {
        isGameOver = true;
        stopTimers();

        if (success) {
            tvStatusInfo.setText("VICTORY");
            netManager.broadcast("CMD|VICTORY");
        } else {
            tvStatusInfo.setText("GAME OVER: " + reason);
            netManager.broadcast("CMD|LOCKDOWN");
        }
        netManager.broadcast("CMD|END_GAME");
        new Handler(Looper.getMainLooper()).postDelayed(() -> {
            netManager.closeAll();

            Intent intent;
            if (success) {
                intent = new Intent(GameplayActivity.this, VictoryActivity.class);
            } else {
                intent = new Intent(GameplayActivity.this, GameOverActivity.class);
                intent.putExtra("REASON", reason);
                intent.putExtra("GAME_MODE", gameMode);
                intent.putExtra("SELECTED_DIFFICULTY", gameDifficulty);
            }
            startActivity(intent);
            finish();
        }, 400);
    }

    private void stopTimers() {
        if (clockRunnable != null) uiHandler.removeCallbacks(clockRunnable);
        if (oscillationRunnable != null) uiHandler.removeCallbacks(oscillationRunnable);
        if (revealTargetRunnable != null) uiHandler.removeCallbacks(revealTargetRunnable);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        stopTimers();
        isGameOver = true;
        netManager.broadcast("CMD|END_GAME");
        new Handler(Looper.getMainLooper()).postDelayed(() -> {
            netManager.closeAll();
        }, 500);

    }
}