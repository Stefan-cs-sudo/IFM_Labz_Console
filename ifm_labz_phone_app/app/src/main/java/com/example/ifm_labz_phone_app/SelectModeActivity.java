package com.example.ifm_labz_phone_app;

import android.content.Intent;
import android.os.Bundle;
import android.widget.Button;
import androidx.appcompat.app.AppCompatActivity;

public class SelectModeActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_select_mode);

        Button btnSingle = findViewById(R.id.btnSingle);
        Button btnCoop = findViewById(R.id.btnCoop);

        btnSingle.setOnClickListener(v -> goNext("SINGLE"));
        btnCoop.setOnClickListener(v -> goNext("COOP"));
    }

    private void goNext(String mode) {
        Intent intent = new Intent(SelectModeActivity.this, SelectDifficultyActivity.class);
        intent.putExtra("GAME_MODE", mode);
        startActivity(intent);
        overridePendingTransition(android.R.anim.fade_in, android.R.anim.fade_out);
    }
}