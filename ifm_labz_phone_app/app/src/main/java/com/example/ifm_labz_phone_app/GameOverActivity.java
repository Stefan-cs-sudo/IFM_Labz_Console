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

public class GameOverActivity extends AppCompatActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        EdgeToEdge.enable(this);
        setContentView(R.layout.activity_game_over);
        ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main), (v, insets) -> {
            Insets systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars());
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom);
            return insets;
        });

        TextView tvLoseContent = findViewById(R.id.tvLoseContent);
        tvLoseContent.setText(Html.fromHtml(tvLoseContent.getText().toString(), Html.FROM_HTML_MODE_LEGACY));

        Button btnRetry = findViewById(R.id.btnRetry);
        if (btnRetry != null) {
            btnRetry.setOnClickListener(v -> {
                Intent intent = new Intent(GameOverActivity.this, LobbyActivity.class);

                intent.putExtra("GAME_MODE", getIntent().getStringExtra("GAME_MODE"));
                intent.putExtra("SELECTED_DIFFICULTY", getIntent().getStringExtra("SELECTED_DIFFICULTY"));

                startActivity(intent);
                overridePendingTransition(android.R.anim.fade_in, android.R.anim.fade_out);
            });
        }
    }
}
