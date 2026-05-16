package com.example.ifm_labz_phone_app;

import android.content.Intent;
import android.os.Bundle;
import android.text.Html;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import androidx.activity.EdgeToEdge;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

public class StoryActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        EdgeToEdge.enable(this);
        setContentView(R.layout.activity_story);

        ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main), (v, insets) -> {
            Insets systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars());
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom);
            return insets;
        });

        TextView tvStoryContent = findViewById(R.id.tvStoryContent);
        tvStoryContent.setMovementMethod(new android.text.method.ScrollingMovementMethod());
        if (tvStoryContent != null) {
            tvStoryContent.setText(Html.fromHtml(tvStoryContent.getText().toString(), Html.FROM_HTML_MODE_LEGACY));
        }

        final String selectedDifficulty = getIntent().getStringExtra("SELECTED_DIFFICULTY") != null ?
                getIntent().getStringExtra("SELECTED_DIFFICULTY") : "MEDIUM";

        final String gameMode = getIntent().getStringExtra("GAME_MODE") != null ?
                getIntent().getStringExtra("GAME_MODE") : "SINGLE";

        Button btnBeginBypass = findViewById(R.id.btnBeginBypass);
        if (btnBeginBypass != null) {
            btnBeginBypass.setOnClickListener(v -> {
                Intent intent = new Intent(StoryActivity.this, LobbyActivity.class);
                intent.putExtra("SELECTED_DIFFICULTY", selectedDifficulty);
                intent.putExtra("GAME_MODE", gameMode);
                startActivity(intent);
                finish();
                overridePendingTransition(android.R.anim.fade_in, android.R.anim.fade_out);
            });
        }
    }
}