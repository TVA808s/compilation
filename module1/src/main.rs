use regex::Regex;
use std::fs;
use std::io::{self, Write};

fn main() -> io::Result<()> {
    let input_file = "test.rs";
    // let input_file = "test_with_errors.rs";
    let output_file = "test_cleaned.rs";
    
    // Проверка существования файла
    if !std::path::Path::new(input_file).exists() {
        eprintln!("ОШИБКА: Файл '{}' не найден!", input_file);
        return Ok(());
    }
    
    // Чтение файла
    let content = match fs::read_to_string(input_file) {
        Ok(text) => text,
        Err(e) => {
            eprintln!("ОШИБКА: Не удалось прочитать файл: {}", e);
            return Ok(());
        }
    };
    
    let errors = check_preprocessing_errors(&content);
    
    if !errors.is_empty() {
        println!("\nНАЙДЕНЫ ОШИБКИ ПРЕПРОЦЕССИНГА:");
        for error in errors {
            println!("   {}", error);
        }
        println!("\nОчистка кода будет продолжена, но результат может быть некорректным.\n");
    } else {
        println!("Ошибок не выявленно\n");
    }
    
    // Очистка кода
    let cleaned = clean_code(&content);
    
    // Сохранение результата
    fs::write(output_file, &cleaned);
    
    print_preview(&cleaned);
    
    Ok(())
}

/// Проверка ошибок препроцессинга (только комментарии и форматирование)
fn check_preprocessing_errors(content: &str) -> Vec<String> {
    let mut errors = Vec::new();
    let lines: Vec<&str> = content.lines().collect();
    
    // 1. ПРОВЕРКА НЕЗАКРЫТЫХ МНОГОСТРОЧНЫХ КОММЕНТАРИЕВ
    let mut in_multiline_comment = false;
    let mut comment_start_line = 0;
    let mut comment_start_col = 0;
    
    for (line_num, line) in lines.iter().enumerate() {
        let mut chars: Vec<char> = line.chars().collect();
        let mut i = 0;
        
        while i < chars.len() {
            if !in_multiline_comment {
                // Ищем открытие комментария /*
                if i + 1 < chars.len() && chars[i] == '/' && chars[i + 1] == '*' {
                    in_multiline_comment = true;
                    comment_start_line = line_num + 1;
                    comment_start_col = i + 1;
                    i += 2;
                    continue;
                }
            } else {
                // Ищем закрытие комментария */
                if i + 1 < chars.len() && chars[i] == '*' && chars[i + 1] == '/' {
                    in_multiline_comment = false;
                    i += 2;
                    continue;
                }
            }
            i += 1;
        }
    }
    
    if in_multiline_comment {
        errors.push(format!(
            "ОШИБКА: Незакрытый многострочный комментарий (начат на строке {}, позиция {})",
            comment_start_line, comment_start_col
        ));
    }
    
    // 2. ПРОВЕРКА НЕДОПУСТИМЫХ СИМВОЛОВ В КОММЕНТАРИЯХ
    let mut in_single_comment = false;
    let mut in_multi_comment = false;
    
    for (line_num, line) in lines.iter().enumerate() {
        let mut chars: Vec<char> = line.chars().collect();
        let mut i = 0;
        in_single_comment = false;
        
        while i < chars.len() {
            // Проверяем состояние комментариев
            if !in_multi_comment && !in_single_comment {
                if i + 1 < chars.len() && chars[i] == '/' && chars[i + 1] == '/' {
                    in_single_comment = true;
                    i += 2;
                    continue;
                }
                if i + 1 < chars.len() && chars[i] == '/' && chars[i + 1] == '*' {
                    in_multi_comment = true;
                    i += 2;
                    continue;
                }
            }
            
            if i + 1 < chars.len() && chars[i] == '*' && chars[i + 1] == '/' && in_multi_comment {
                in_multi_comment = false;
                i += 2;
                continue;
            }
            
            // Проверяем символы ТОЛЬКО внутри комментариев
            if in_single_comment || in_multi_comment {
                let ch = chars[i];
                let ch_code = ch as u32;
                
                // Недопустимые символы в комментариях:
                // - Управляющие символы (кроме \t, \n)
                // - Нулевой символ
                // - Специальные непечатные символы
                if ch_code == 0 {
                    errors.push(format!(
                        "НЕДОПУСТИМЫЙ СИМВОЛ: нулевой символ '\\0' в комментарии (строка {})",
                        line_num + 1
                    ));
                } else if ch_code < 9 || (ch_code > 13 && ch_code < 32) {
                    errors.push(format!(
                        "НЕДОПУСТИМЫЙ СИМВОЛ: управляющий символ (код {}) в комментарии (строка {})",
                        ch_code, line_num + 1
                    ));
                }
            }
            
            i += 1;
        }
    }
    
    // 3. ПРОВЕРКА ВЛОЖЕННЫХ МНОГОСТРОЧНЫХ КОММЕНТАРИЕВ
    let mut comment_depth = 0;
    let mut nested_errors = Vec::new();
    
    for (line_num, line) in lines.iter().enumerate() {
        let mut chars: Vec<char> = line.chars().collect();
        let mut i = 0;
        
        while i < chars.len() {
            if i + 1 < chars.len() && chars[i] == '/' && chars[i + 1] == '*' {
                comment_depth += 1;
                if comment_depth > 1 {
                    nested_errors.push(format!(
                        "ВЛОЖЕННЫЙ КОММЕНТАРИЙ: обнаружен вложенный многострочный комментарий (строка {}, глубина {})",
                        line_num + 1, comment_depth
                    ));
                }
                i += 2;
                continue;
            }
            if i + 1 < chars.len() && chars[i] == '*' && chars[i + 1] == '/' {
                comment_depth -= 1;
                i += 2;
                continue;
            }
            i += 1;
        }
    }
    
    if comment_depth > 0 {
        errors.push(format!(
            "ОШИБКА: Незакрытый многострочный комментарий (глубина {})",
            comment_depth
        ));
    }
    
    errors.extend(nested_errors);
    
    errors
}

fn clean_code(code: &str) -> String {
    // 1. Удаляем многострочные комментарии
    let multiline_comment = Regex::new(r"/\*[\s\S]*?\*/").unwrap();
    let without_multiline = multiline_comment.replace_all(code, "");
    
    // 2. Удаляем однострочные комментарии
    let singleline_comment = Regex::new(r"//[^\n]*").unwrap();
    let without_comments = singleline_comment.replace_all(&without_multiline, "");
    
    // 3. Удаляем пробелы в начале и конце строк
    let trailing_spaces = Regex::new(r"[ \t]+$").unwrap();
    let leading_spaces = Regex::new(r"^[ \t]+").unwrap();
    let tabs = Regex::new(r"\t").unwrap();
    
    let mut lines: Vec<String> = without_comments
        .lines()
        .map(|line| {
            let line = tabs.replace_all(line, "    ");
            let line = leading_spaces.replace_all(&line, "");
            let line = trailing_spaces.replace_all(&line, "");
            line.to_string()
        })
        .collect();
    
    // 4. Удаляем пустые строки
    lines.retain(|line| !line.is_empty());
    
    // 5. Объединяем строки
    let result = lines.join("\n");
    
    // 6. Сжимаем множественные пробелы
    let multiple_spaces = Regex::new(r"[ ]+").unwrap();
    let result = multiple_spaces.replace_all(&result, " ").to_string();
    
    result
}

fn print_preview(cleaned: &str) {
    let cleaned_lines: Vec<&str> = cleaned.lines().collect();
    for (i, line) in cleaned_lines.iter().enumerate() {
        if !line.is_empty() {
            let display_line = if line.len() > 60 {
                format!("{}...", &line[..60])
            } else {
                line.to_string()
            };
            println!("{:2}: {}", i + 1, display_line);
        }
    }
}