#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <chrono>

using namespace geode::prelude;

namespace {
    // Settings are cached here and refreshed by listeners, so the per-frame
    // hooks never touch the settings system.
    struct Config {
        bool enabled = true;
        bool noParticles = true;
        bool noTrails = false;
        bool noShake = true;
        bool hideBackground = false;
        bool purgeTextures = false;
        bool overlay = false;
        int corner = 0;
        float overlayScale = 0.4f;
    };
    Config g;

    void reloadConfig() {
        auto* m = Mod::get();
        g.enabled        = m->getSettingValue<bool>("enabled");
        g.noParticles    = m->getSettingValue<bool>("disable-particles");
        g.noTrails       = m->getSettingValue<bool>("disable-trails");
        g.noShake        = m->getSettingValue<bool>("disable-shake");
        g.hideBackground = m->getSettingValue<bool>("hide-background");
        g.purgeTextures  = m->getSettingValue<bool>("purge-textures");
        g.overlay        = m->getSettingValue<bool>("fps-overlay");

        auto corner = m->getSettingValue<std::string>("overlay-corner");
        g.corner = corner == "Top left" ? 0
                 : corner == "Top right" ? 1
                 : corner == "Bottom left" ? 2 : 3;

        g.overlayScale = static_cast<float>(m->getSettingValue<double>("overlay-scale"));
    }

    void quiet(CCParticleSystemQuad* system) {
        if (!system) return;
        system->stopSystem();
        system->setVisible(false);
    }

    void trim(PlayerObject* player) {
        if (!player) return;

        if (g.noParticles) {
            quiet(player->m_playerGroundParticles);
            quiet(player->m_trailingParticles);
            quiet(player->m_shipClickParticles);
            quiet(player->m_vehicleGroundParticles);
            quiet(player->m_ufoClickParticles);
            quiet(player->m_robotBurstParticles);
            quiet(player->m_dashParticles);
            quiet(player->m_swingBurstParticles1);
            quiet(player->m_swingBurstParticles2);
            quiet(player->m_landParticles0);
            quiet(player->m_landParticles1);
        }

        if (g.noTrails) {
            if (player->m_regularTrail) player->m_regularTrail->setVisible(false);
            if (player->m_waveTrail) player->m_waveTrail->setVisible(false);
        }
    }
}

$on_mod(Loaded) {
    reloadConfig();

    for (auto const* key : {
        "enabled", "disable-particles", "disable-trails", "disable-shake",
        "hide-background", "purge-textures", "fps-overlay"
    }) {
        listenForSettingChanges<bool>(key, [](bool) { reloadConfig(); });
    }
    listenForSettingChanges<std::string>("overlay-corner", [](std::string) { reloadConfig(); });
    listenForSettingChanges<double>("overlay-scale", [](double) { reloadConfig(); });
}

class $modify(PerfPlayLayer, PlayLayer) {
    struct Fields {
        CCLabelBMFont* label = nullptr;
        int appliedCorner = -1;
        float appliedScale = -1.f;
        bool bgHidden = false;
        bool started = false;
        int frames = 0;
        float worstMs = 0.f;
        std::chrono::steady_clock::time_point windowStart{};
        std::chrono::steady_clock::time_point lastFrame{};
    };

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        if (g.enabled) {
            trim(m_player1);
            trim(m_player2);
            this->updateBackground();
        }
        else if (m_fields->bgHidden) {
            this->updateBackground();
        }

        this->updateOverlay();
    }

    void updateBackground() {
        auto* f = m_fields.self();
        if (!m_background) return;

        if (g.enabled && g.hideBackground) {
            if (m_background->isVisible()) m_background->setVisible(false);
            f->bgHidden = true;
        }
        else if (f->bgHidden) {
            m_background->setVisible(true);
            f->bgHidden = false;
        }
    }

    void layoutOverlay() {
        auto* f = m_fields.self();
        if (!f->label) return;

        auto win = CCDirector::get()->getWinSize();
        constexpr float pad = 6.f;
        bool right = g.corner == 1 || g.corner == 3;
        bool top = g.corner == 0 || g.corner == 1;

        f->label->setScale(g.overlayScale);
        f->label->setAnchorPoint({ right ? 1.f : 0.f, top ? 1.f : 0.f });
        f->label->setPosition({
            right ? win.width - pad : pad,
            top ? win.height - pad : pad
        });

        f->appliedCorner = g.corner;
        f->appliedScale = g.overlayScale;
    }

    void updateOverlay() {
        auto* f = m_fields.self();

        if (!g.overlay) {
            if (f->label) {
                f->label->removeFromParent();
                f->label = nullptr;
                f->started = false;
            }
            return;
        }

        if (!f->label) {
            if (!m_uiLayer) return;
            f->label = CCLabelBMFont::create("-- FPS", "bigFont.fnt");
            f->label->setOpacity(190);
            m_uiLayer->addChild(f->label, 1000);
            this->layoutOverlay();
        }
        else if (f->appliedCorner != g.corner || f->appliedScale != g.overlayScale) {
            this->layoutOverlay();
        }

        auto now = std::chrono::steady_clock::now();
        if (!f->started) {
            f->started = true;
            f->windowStart = now;
            f->lastFrame = now;
            f->frames = 0;
            f->worstMs = 0.f;
            return;
        }

        float ms = std::chrono::duration<float, std::milli>(now - f->lastFrame).count();
        f->lastFrame = now;

        // A gap this long means the game was paused, so restart the window.
        if (ms > 500.f) {
            f->windowStart = now;
            f->frames = 0;
            f->worstMs = 0.f;
            return;
        }

        f->frames++;
        if (ms > f->worstMs) f->worstMs = ms;

        float elapsed = std::chrono::duration<float>(now - f->windowStart).count();
        if (elapsed >= 0.5f) {
            float fps = static_cast<float>(f->frames) / elapsed;
            float avgMs = 1000.f * elapsed / static_cast<float>(f->frames);
            auto text = fmt::format("{} FPS  {:.1f} ms  (worst {:.1f})",
                static_cast<int>(fps + 0.5f), avgMs, f->worstMs);
            f->label->setString(text.c_str());

            f->windowStart = now;
            f->frames = 0;
            f->worstMs = 0.f;
        }
    }
};

class $modify(PerfBaseGameLayer, GJBaseGameLayer) {
    void shakeCamera(float duration, float strength, float interval) {
        if (g.enabled && g.noShake) return;
        GJBaseGameLayer::shakeCamera(duration, strength, interval);
    }
};

class $modify(PerfMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        if (g.enabled && g.purgeTextures) {
            // Only drops textures with no owner, so nothing in use is affected.
            CCTextureCache::sharedTextureCache()->removeUnusedTextures();
        }
        return true;
    }
};
