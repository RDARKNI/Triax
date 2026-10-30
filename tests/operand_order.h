#ifndef TRIAX_OPERAND_ORDER_H
#define TRIAX_OPERAND_ORDER_H

// Shared by the C and C++ runners; validate_formats.sh checks the child JSON
// and text so an accidental operand reversal cannot pass on outcome alone.
triaxi_validate(operand_order, expectations, TRIAXI_VALIDATE_FAILED) {
  int           actual = 7, expected = 9;
  int           actual_array[] = {7}, expected_array[] = {9};
  unsigned char actual_bytes[] = {0x11}, expected_bytes[] = {0x22};
  triax_expect_eq(actual, expected);
  triax_expect_streq("actual", "expected");
  triax_expect_floateq_abstol(7.0, 9.0, 0.5);
  triax_expect_floateq_reltol(7.0, 9.0, 0.125);
  triax_expect_arreq(actual_array, expected_array, 1);
  triax_expect_memeq(actual_bytes, expected_bytes, 1);
}
triaxi_validate(operand_order, fatal, TRIAXI_VALIDATE_FAILED) {
  int actual = 7, expected = 9;
  triax_assert_eq(actual, expected);
  triax_expect(0, "fatal assertion must stop execution");
}

#endif
