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

/// Проверка ошибок препроцессинга
fn check_preprocessing_errors(content: &str) -> Vec<String> {
    let mut errors = Vec::new();
    let lines: Vec<&str> = content.lines().collect();
    
    let mut in_multi_comment = false;
    let mut comment_start_line = 0;
    let mut comment_depth = 0;  // Для вложенных комментариев
    
    for (line_num, line) in lines.iter().enumerate() {
        let chars: Vec<char> = line.chars().collect();
        let mut i = 0;
        let mut in_single_comment = false;  // Сбрасывается для каждой строки
        
        while i < chars.len() {
            // === ОБРАБОТКА КОММЕНТАРИЕВ ===
            
            // Открытие однострочного //
            if !in_multi_comment && !in_single_comment {
                if i + 1 < chars.len() && chars[i] == '/' && chars[i + 1] == '/' {
                    in_single_comment = true;
                    i += 2;
                    continue;
                }
            }
            
            // Открытие многострочного /*
            if !in_single_comment {
                if i + 1 < chars.len() && chars[i] == '/' && chars[i + 1] == '*' {
                    if !in_multi_comment {
                        comment_start_line = line_num + 1;
                    }
                    in_multi_comment = true;
                    comment_depth += 1;
                    
                    if comment_depth > 1 {
                        errors.push(format!(
                            "ВЛОЖЕННЫЙ КОММЕНТАРИЙ: строка {}, глубина {}",
                            line_num + 1, comment_depth
                        ));
                    }
                    i += 2;
                    continue;
                }
            }
            
            // Закрытие многострочного */
            if i + 1 < chars.len() && chars[i] == '*' && chars[i + 1] == '/' {
                if in_multi_comment {
                    comment_depth -= 1;
                    if comment_depth == 0 {
                        in_multi_comment = false;
                    }
                } else if !in_single_comment {
                    // */ без открывающего /*
                    errors.push(format!(
                        "ОШИБКА: */ без открывающего /* (строка {}, позиция {})",
                        line_num + 1, i + 1
                    ));
                }
                i += 2;
                continue;
            }
            i += 1;
        }
        // in_single_comment автоматически сбрасывается в конце строки
    }
    
    // Проверка незакрытого комментария в конце файла
    if in_multi_comment {
        errors.push(format!(
            "ОШИБКА: Незакрытый многострочный комментарий (начат на строке {})",
            comment_start_line
        ));
    }
    
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
        line.to_string();
        println!("{}", line);
    }
}