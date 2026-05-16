package com.example.ifm_labz_phone_app;

import android.content.Intent;
import android.os.Bundle;
import android.widget.Button;
import androidx.appcompat.app.AppCompatActivity;

public class SelectDifficultyActivity extends AppCompatActivity {

    private String gameMode = "SINGLE";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_select_difficulty);

        if (getIntent() != null && getIntent().hasExtra("GAME_MODE")) {
            gameMode = getIntent().getStringExtra("GAME_MODE");
        }

        Button btnEasy = findViewById(R.id.easyBtn);
        Button btnMedium = findViewById(R.id.mediumBtn);
        Button btnHard = findViewById(R.id.hardBtn);

        btnEasy.setOnClickListener(v -> proceedToStory("EASY"));
        btnMedium.setOnClickListener(v -> proceedToStory("MEDIUM"));
        btnHard.setOnClickListener(v -> proceedToStory("HARD"));
    }

    private void proceedToStory(String difficultyString) {
        Intent intent = new Intent(SelectDifficultyActivity.this, StoryActivity.class);

        intent.putExtra("SELECTED_DIFFICULTY", difficultyString);
        intent.putExtra("GAME_MODE", gameMode);

        startActivity(intent);

        overridePendingTransition(android.R.anim.fade_in, android.R.anim.fade_out);
    }
}