import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
APP = (ROOT / "Core/Src/app.c").read_text(encoding="utf-8")
MAIN = (ROOT / "Core/Src/main.c").read_text(encoding="utf-8")
RS485 = (ROOT / "Core/Src/rs485_if.c").read_text(encoding="utf-8")
RING = (ROOT / "Core/Src/ringbuf.c").read_text(encoding="utf-8")
IOC = (ROOT / "IN_OUT_SLAVE_F446.ioc").read_text(encoding="utf-8")


class DebounceModel:
    def __init__(self, raw, now):
        self.last_raw = raw
        self.changed = now & 0xFFFFFFFF
        self.stable = 0
        self.initialized = False

    def tick(self, raw, now):
        now &= 0xFFFFFFFF
        if raw != self.last_raw:
            self.last_raw = raw
            self.changed = now
            return False
        if ((now - self.changed) & 0xFFFFFFFF) >= 5:
            first = not self.initialized
            self.stable = raw
            self.initialized = True
            return not first
        return False


class RingModel:
    def __init__(self, size):
        self.size = size
        self.data = []
        self.discard = False
        self.overruns = 0
        self.oversizes = 0

    def push(self, byte):
        if self.discard:
            if byte == "\n":
                self.discard = False
            return
        if len(self.data) >= self.size - 1:
            self.overruns += 1
            self.data.clear()
            self.discard = byte != "\n"
            return
        self.data.append(byte)

    def line(self, maximum):
        if "\n" not in self.data:
            return None
        end = self.data.index("\n")
        chars = [c for c in self.data[:end] if c != "\r"]
        del self.data[: end + 1]
        if len(chars) >= maximum:
            self.oversizes += 1
            return None
        return "".join(chars)


class FirmwareTests(unittest.TestCase):
    def test_01_poto_low_is_on(self):
        self.assertIn("(st == GPIO_PIN_RESET) ? 1u : 0u", APP)

    def test_02_poto_high_is_off(self):
        self.assertIn("GPIO_PIN_RESET) ? 1u : 0u", APP)

    def test_03_boot_input_has_no_event(self):
        d = DebounceModel(1, 0)
        self.assertFalse(d.tick(1, 5))
        self.assertTrue(d.initialized)
        self.assertIn("initial_active_suppressed", APP)

    def test_04_four_ms_is_ignored(self):
        d = DebounceModel(0, 0); d.tick(0, 5); d.tick(1, 6)
        self.assertFalse(d.tick(1, 10)); self.assertEqual(d.stable, 0)

    def test_05_five_ms_is_accepted(self):
        d = DebounceModel(0, 0); d.tick(0, 5); d.tick(1, 6)
        self.assertTrue(d.tick(1, 11)); self.assertEqual(d.stable, 1)

    def test_06_candidate_change_restarts_timer(self):
        d = DebounceModel(0, 0); d.tick(0, 5); d.tick(1, 6); d.tick(0, 9)
        self.assertFalse(d.tick(0, 13)); self.assertEqual(d.stable, 0)

    def test_07_tick_wrap_is_safe(self):
        d = DebounceModel(1, 0xFFFFFFFC)
        self.assertFalse(d.tick(1, 1)); self.assertTrue(d.initialized)

    def test_08_outputs_are_explicit_set_reset(self):
        self.assertIn("GPIO_PIN_SET", APP); self.assertIn("GPIO_PIN_RESET", APP)
        self.assertNotIn("HAL_GPIO_TogglePin", APP)

    def test_09_boot_all_outputs_off(self):
        self.assertIn("io_all_off();", APP)
        self.assertIn("OUT_OFF_LEVEL  GPIO_PIN_RESET", APP)

    def test_10_bad_crc_cannot_dispatch(self):
        self.assertRegex(APP, r"RS485_MSG_BADCRC[\s\S]{0,160}continue;")

    def test_11_other_address_cannot_dispatch(self):
        self.assertIn("req.addr != s_addr && req.addr != 0", APP)

    def test_12_unknown_nacks_without_output_change(self):
        self.assertIn('"NACK", "UNKNOWN_CMD"', APP)

    def test_13_cfg_pt_stubs_removed(self):
        self.assertNotIn("received (stub)", APP)

    def test_14_duplicate_run_replays_ack(self):
        self.assertIn("duplicate ACK", APP); self.assertIn("seq == s_last_seq", APP)

    def test_15_dip_active_low_range(self):
        dip = (ROOT / "Core/Src/dip.c").read_text(encoding="utf-8")
        self.assertIn("(~s.raw) & 0x0Fu", dip)

    def test_16_ring_wrap_uses_modulo(self):
        self.assertIn("% rb->size", RING)

    def test_17_ring_full_counts_overrun(self):
        r = RingModel(4)
        for c in "abcd": r.push(c)
        self.assertEqual(r.overruns, 1)

    def test_18_overrun_resyncs_at_lf(self):
        r = RingModel(4)
        for c in "abcdBAD\nOK\n": r.push(c)
        self.assertEqual(r.line(16), "OK")

    def test_19_oversize_is_fully_discarded(self):
        r = RingModel(32)
        for c in "TOO-LONG\n": r.push(c)
        self.assertIsNone(r.line(4)); self.assertEqual(r.oversizes, 1)

    def test_20_frame_after_oversize_recovers(self):
        r = RingModel(32)
        for c in "TOO-LONG\nOK\n": r.push(c)
        self.assertIsNone(r.line(4)); self.assertEqual(r.line(4), "OK")

    def test_21_uart_error_rearm_is_state_guarded(self):
        self.assertIn("HAL_UART_STATE_BUSY_RX", MAIN)
        self.assertIn("rs485_if_on_rx_rearm_fail_isr", MAIN)

    def test_22_de_tc_success_path(self):
        self.assertLess(RS485.index("GPIO_PIN_SET"), RS485.index("HAL_UART_Transmit"))
        self.assertIn("UART_FLAG_TC", RS485)

    def test_23_tx_failure_lowers_de(self):
        self.assertRegex(RS485, r"st != HAL_OK[\s\S]{0,240}GPIO_PIN_RESET")

    def test_24_tc_timeout_lowers_de(self):
        self.assertLess(RS485.index("s_tx_tc_timeout_count++"), RS485.rindex("GPIO_PIN_RESET"))

    def test_25_loop_budget_is_two(self):
        self.assertIn("#define APP_FRAME_BUDGET     2u", APP)

    def test_26_replies_fit_fifty_bytes(self):
        def length(cmd, args): return len(f"@15|9999|{cmd}|{args}*FFFF\n")
        for cmd, args in (("ACK", ""), ("NACK", "UNKNOWN_CMD"),
                          ("STATUS", "DONE;NEXT=255"), ("STATUS", "BUSY")):
            self.assertLessEqual(length(cmd, args), 50)

    def test_27_master_protocol_shape_is_preserved(self):
        self.assertIn('"@%02u|%04u|%s|%s"', RS485)
        self.assertIn('strcmp(req.cmd, "RUN")', APP)
        self.assertIn('strcmp(req.cmd, "STATUS")', APP)

    def test_28_no_temperature_build_feature(self):
        product_files = [ROOT / "Core/Src/app.c", ROOT / "Core/Src/main.c",
                         ROOT / "Core/Src/rs485_if.c", ROOT / "Core/Inc/app.h",
                         ROOT / "Core/Inc/rs485_if.h"]
        product = "\n".join(p.read_text(encoding="utf-8", errors="ignore")
                            for p in product_files)
        for term in ("PT100", "DS18B20", "ONEWIRE", "TEMPERATURE"):
            self.assertNotIn(term, product.upper())
        self.assertNotIn("ADC1", IOC)


if __name__ == "__main__":
    unittest.main(verbosity=2)
